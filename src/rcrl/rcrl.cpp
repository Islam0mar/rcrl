#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "rcrl.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "config.h"
#include "rcrl_parser.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HMODULE RCRL_Dynlib;
#define RDRL_LoadDynlib(lib) LoadLibrary(lib)
#define RCRL_CloseDynlib FreeLibrary
#define RCRL_CopyDynlib(src, dst) CopyFile(src, dst, false)
#define RCRL_System_Delete "del /Q "

#else

#include <dlfcn.h>
typedef void* RCRL_Dynlib;
#define RDRL_LoadDynlib(lib) dlopen(lib, RTLD_NOW)
#define RCRL_CloseDynlib dlclose
#define RCRL_CopyDynlib(src, dst) \
  (!system((string("cp ") + src + " " + dst).c_str()))
#define RCRL_System_Delete "rm "

#endif

namespace fs = std::filesystem;
using std::cerr;
using std::cout;
using std::endl;
using std::string;

namespace rcrl {

constexpr auto kOutputFileName = "out.txt";

Plugin::Plugin(string file_base_name, std::vector<const char*> flags)
    : parser_(file_base_name + ".cpp", flags) {
  auto header = GetHeaderNameFromSourceName(parser_.get_file_name());
  std::ofstream f(header, std::fstream::trunc | std::fstream::out);
  f << "#pragma once\n";
  f.close();
  fs::path output_file = fs::current_path() / kOutputFileName;
  fs::directory_entry entry_output_file{output_file};
  if (!entry_output_file.exists()) {
    std::ofstream(output_file.c_str()).put('a');  // create file
  }
  out_err_file_last_modification_time_ = fs::last_write_time(output_file);
  // change time to make iscomiling work
  std::ofstream(output_file.c_str()).put('a');
}

string Plugin::get_new_compiler_output() {
  auto val = compiler_output_;
  compiler_output_.clear();
  return val;
}

string Plugin::CleanupPlugins(bool redirect_stdout) {
  assert(!IsCompiling());

  if (redirect_stdout)
    freopen(RCRL_BUILD_FOLDER "/rcrl_stdout.txt", "w", stdout);

  // close the plugins_ in reverse order
  for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it)
    RCRL_CloseDynlib(it->second);

  string out;

  if (redirect_stdout) {
    fclose(stdout);
    freopen("CON", "w", stdout);

    FILE* f = fopen(RCRL_BUILD_FOLDER "/rcrl_stdout.txt", "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    out.resize(fsize);
    fread((void*)out.data(), fsize, 1, f);
    fclose(f);
  }

  string bin_folder(RCRL_BIN_FOLDER);
#ifdef _WIN32
  // replace forward slash with windows style slash
  replace(bin_folder.begin(), bin_folder.end(), '/', '\\');
#endif  // _WIN32

  if (plugins_.size())
    system((string(RCRL_System_Delete) + bin_folder + "lib" +
            RCRL_PLUGIN_NAME "_*" RCRL_EXTENSION)
               .c_str());
  plugins_.clear();

  return out;
}

bool Plugin::CompileCode(string code) {
  assert(!IsCompiling());
  assert(code.size());

  // fix line endings
  replace(code.begin(), code.end(), '\r', '\n');

  // figure out the sections
  std::fstream file(parser_.get_file_name(),
                    std::fstream::in | std::fstream::out | std::fstream::trunc);
  // add header to correctly parse the input
  auto header = GetHeaderNameFromSourceName(parser_.get_file_name());
  file << "#include \"" + header + "\"\n" << code;
  file.close();
  parser_.Reparse();
  parser_.GenerateSourceFile(parser_.get_file_name(),
                             "#include \"" + header + "\"\n");
  // mark the successful compilation flag as false
  last_compile_successful_ = false;
  future_ = std::async(std::launch::async, [&]() -> int {
    fs::path output_file = fs::current_path() / kOutputFileName;
    out_err_file_last_modification_time_ = fs::last_write_time(output_file);
    auto cmd = string("cmake --build . --target " +
                      GetBaseNameFromSourceName(parser_.get_file_name()) +
                      " 1>" + string(kOutputFileName) + " 2>&1");
    return system(cmd.c_str());
  });
  return true;
}

bool Plugin::IsCompiling() {
  fs::path output_file = fs::current_path() / kOutputFileName;
  return out_err_file_last_modification_time_ ==
         fs::last_write_time(output_file);
}

bool Plugin::TryGetExitStatusFromCompile(int& exit_code) {
  if (future_.valid() && !IsCompiling()) {
    // int result = compiler_process->exit_code();
    // std::cout << "Signaled with sginal #" << result << ", aka "
    //           << strsignal(result) << endl;

    exit_code = future_.get();
    // append compiler out and error
    std::ifstream f(kOutputFileName);
    std::stringstream ss;
    ss << f.rdbuf();
    compiler_output_.append(ss.str());
    if ((last_compile_successful_ = exit_code == 0)) {
      auto header = GetHeaderNameFromSourceName(parser_.get_file_name());
      parser_.GenerateHeaderFile(header);
    }
    return true;
  }
  return false;
}
string Plugin::CopyAndLoadNewPlugin(bool redirect_stdout) {
  assert(!IsCompiling());
  assert(last_compile_successful_);

  last_compile_successful_ =
      false;  // shouldn't call this function twice in a
              // row without compiling anything in between
  // copy the plugin
  auto name_copied = string(RCRL_BIN_FOLDER) + "lib" + RCRL_PLUGIN_NAME "_" +
                     std::to_string(plugins_.size()) + RCRL_EXTENSION;
  const auto copy_res = RCRL_CopyDynlib(
      (string(RCRL_BIN_FOLDER) + "lib" + RCRL_PLUGIN_NAME RCRL_EXTENSION)
          .c_str(),
      name_copied.c_str());
  std::cout << name_copied << "\n";
  assert(copy_res);

  if (redirect_stdout)
    freopen(RCRL_BUILD_FOLDER "/rcrl_stdout.txt", "w", stdout);

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
    fclose(stdout);
    freopen("CON", "w", stdout);

    FILE* f = fopen(RCRL_BUILD_FOLDER "/rcrl_stdout.txt", "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    out.resize(fsize);
    fread((void*)out.data(), fsize, 1, f);
    fclose(f);
  }

  return out;
}

}  // namespace rcrl
