#pragma once

#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <string>
#include <vector>

#include "rcrl_parser.h"

using std::string;
namespace rcrl {

class Plugin {
 public:
  Plugin(string source_file_base_name = "plugin",
         std::vector<string> flags = std::vector<string>(0));
  string get_new_compiler_output();
  string CleanupPlugins(bool redirect_stdout = false);
  bool CompileCode(string code);
  bool IsCompiling();

  bool TryGetExitStatusFromCompile(int& exitcode);
  string CopyAndLoadNewPlugin(bool redirect_stdout = false);
  void set_flags(const std::vector<string>& new_flags);
  ~Plugin();

 private:
  // global state
  std::vector<std::pair<string, void*>> plugins_;
  string compiler_output_;
  std::mutex compiler_output_mut_;
  bool is_compiling_;
  std::future<int> compiler_process_;
  bool last_compile_successful_ = false;
  PluginParser parser_;
};

}  // namespace rcrl
