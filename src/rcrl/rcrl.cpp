#include <cstdio>
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "config.h"
#include "rcrl.h"
#include "rcrl_parser.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HMODULE RCRL_Dynlib;
#define RDRL_LoadDynlib(lib) LoadLibrary(lib)
#define RCRL_CloseDynlib FreeLibrary
#else

#include <dlfcn.h>
typedef void* RCRL_Dynlib;
#define RDRL_LoadDynlib(lib) dlopen(lib, RTLD_LAZY | RTLD_GLOBAL)
#define RCRL_CloseDynlib dlclose
#endif

namespace bp = boost::process;
using std::cerr;
using std::endl;
using std::string;

namespace rcrl {

auto CopyFileToString(const string& f_name) {
  FILE* f = fopen(f_name.c_str(), "rb");
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  string out;
  out.resize(fsize);
  fread((void*)out.data(), fsize, 1, f);
  fclose(f);
  return out;
}
Plugin::Plugin(fs::path file, std::vector<string> flags)
    : is_compiling_(false), parser_(file.string() + ".cpp", flags) {
  auto header = parser_.get_file().replace_extension(".hpp");
  std::ofstream f(header, std::fstream::trunc | std::fstream::out);
  f << "#pragma once\n";
  f.close();
}
Plugin::~Plugin() { CleanupPlugins(); }
void Plugin::set_flags(const std::vector<string>& new_flags) {
  // avoid calling it's constructor at the function end
  static std::future<bool> p;
  assert(!IsCompiling());
  is_compiling_ = true;
  p = std::async(std::launch::async, [&]() {
    parser_.set_flags(new_flags);
    return (is_compiling_ = false);
  });
}

string Plugin::get_new_compiler_output() {
  std::lock_guard<std::mutex> lock(compiler_output_mut_);
  auto str = compiler_output_;
  compiler_output_.clear();
  return str;
}

string Plugin::CleanupPlugins(bool redirect_stdout) {
  assert(!IsCompiling());

  int fd;
  fpos_t pos;
  if (redirect_stdout) {
    fgetpos(stdout, &pos);
    fd = dup(fileno(stdout));
    freopen(kRcrlOutputFile.c_str(), "w", stdout);
  }

  // close the plugins_ in reverse order
  for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it)
    RCRL_CloseDynlib(it->second);

  string out;

  if (redirect_stdout) {
    fflush(stdout);
    dup2(fd, fileno(stdout));
    close(fd);
    clearerr(stdout);
    fsetpos(stdout, &pos);

    FILE* f = fopen(kRcrlOutputFile.c_str(), "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    out.resize(fsize);
    fread((void*)out.data(), fsize, 1, f);
    fclose(f);
  }

  for (const auto& [name, _] : plugins_) {
    std::remove(name.c_str());
  }

  plugins_.clear();

  // reset header file
  auto header = parser_.get_file().replace_extension(".hpp");
  std::ofstream f(header, std::fstream::trunc | std::fstream::out);
  f << "#pragma once\n";

  return out;
}

bool Plugin::CompileCode(string code) {
  assert(!IsCompiling());
  assert(code.size());

  // fix line endings
  replace(code.begin(), code.end(), '\r', '\n');

  // figure out the sections
  std::fstream file(parser_.get_file(),
                    std::fstream::in | std::fstream::out | std::fstream::trunc);
  // add header to correctly parse the input
  auto header = parser_.get_file().stem().string() + ".hpp";
  file << "#include \"" + header + "\"\n" << code;
  file.close();
  // mark the successful compilation flag as false
  last_compile_successful_ = false;
  compiler_output_.clear();
  is_compiling_ = true;
  // TODO: add buffer size to config file
  static std::vector<char> buf(128);
  compiler_process_ = std::async(std::launch::async, [&]() {
    // reparsing takes some time so moved inside async
    parser_.Reparse();
    parser_.GenerateSourceFile(parser_.get_file());
    boost::asio::io_service ios;
    bp::async_pipe ap(ios);
    auto output_buffer = boost::asio::buffer(buf);
    // must use clang++ as g++ differ from libclang deduced types
    auto cmd = bp::search_path("clang++").string() + string(" ");
    for (auto flag : parser_.get_flags()) {
      cmd += flag + string(" ");
    }
    cmd +=
        "-shared -Wl,-undefined,error -Wl,-flat_namespace -fvisibility=hidden "
        "-fPIC " +
        parser_.get_file().string() + " -o " +
        std::string(kRcrlOutputDir /
                    (parser_.get_file().stem().string() + ".so"));
    bp::child c(cmd, (bp::std_err & bp::std_out) > ap, bp::std_in.close());
    auto OnStdout = [&](const boost::system::error_code& ec, std::size_t size) {
      auto lambda_impl = [&](const boost::system::error_code& ec, std::size_t n,
                             auto& lambda_ref) {
        std::lock_guard<std::mutex> lock(compiler_output_mut_);
        compiler_output_.reserve(compiler_output_.size() + n);
        compiler_output_.insert(compiler_output_.end(), buf.begin(),
                                buf.begin() + n);
        if (!ec && ap.is_open()) {
          ap.async_read_some(output_buffer,
                             std::bind(lambda_ref, std::placeholders::_1,
                                       std::placeholders::_2, lambda_ref));
        }
      };
      return lambda_impl(ec, size, lambda_impl);
    };
    ap.async_read_some(output_buffer, OnStdout);
    ios.run();
    c.join();
    is_compiling_ = false;
    return c.exit_code();
  });
  return true;
}

bool Plugin::IsCompiling() { return is_compiling_; }

bool Plugin::TryGetExitStatusFromCompile(int& exit_code) {
  if (compiler_process_.valid() && !IsCompiling()) {
    exit_code = compiler_process_.get();
    last_compile_successful_ = (exit_code == 0);
    compiler_output_.append("\nSignaled with sginal #" +
                            std::to_string(exit_code) + ", aka " +
                            strsignal(exit_code) + "\n");
    if (last_compile_successful_) {
      auto header = parser_.get_file().replace_extension(".hpp");
      parser_.GenerateHeaderFile(header);
    }
    return true;
  }
  return false;
}
string Plugin::CopyAndLoadNewPlugin(bool redirect_stdout) {
  assert(!IsCompiling());
  assert(last_compile_successful_);
  is_compiling_ = true;
  last_compile_successful_ =
      false;  // shouldn't call this function twice in a
              // row without compiling anything in between

  // copy the plugin
  const auto name_copied =
      kRcrlOutputDir / (std::string(RCRL_PLUGIN_NAME) + "_" +
                        std::to_string(plugins_.size()) + RCRL_EXTENSION);
  std::error_code copy_res;
  fs::copy(kRcrlOutputDir / (std::string(RCRL_PLUGIN_NAME) + RCRL_EXTENSION),
           name_copied, fs::copy_options::overwrite_existing, copy_res);
  assert(copy_res.value() == 0);
  int fd;
  fpos_t pos;

  if (redirect_stdout) {
    // Save position of current standard output
    fgetpos(stdout, &pos);
    fd = dup(fileno(stdout));
    freopen(kRcrlOutputFile.c_str(), "w", stdout);
  }
  // load the plugin
  auto plugin = RDRL_LoadDynlib(name_copied.c_str());
  if (!plugin) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }
  assert(plugin);

  // add the plugin to the list of loaded ones - for later unloading
  plugins_.push_back({name_copied, plugin});

  string out;

  if (redirect_stdout) {
    // Flush stdout so any buffered messages are delivered
    fflush(stdout);
    // Close file and restore standard output to stdout - which should be the
    // terminal
    dup2(fd, fileno(stdout));
    close(fd);
    clearerr(stdout);
    fsetpos(stdout, &pos);

    FILE* f = fopen(kRcrlOutputFile.c_str(), "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    out.resize(fsize);
    fread((void*)out.data(), fsize, 1, f);
    fclose(f);
  }
  is_compiling_ = false;
  return out;
}

}  // namespace rcrl
