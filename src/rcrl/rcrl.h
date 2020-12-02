#pragma once

#include <filesystem>
#include <future>
#include <string>
#include <vector>

#include "rcrl_parser.h"

using std::string;
namespace rcrl {

class Plugin {
 public:
  Plugin(string source_file_base_name = "plugin",
         std::vector<const char*> flags = std::vector<const char*>(0));
  string get_new_compiler_output();
  string CleanupPlugins(bool redirect_stdout = false);
  bool CompileCode(string code);
  bool IsCompiling();

  bool TryGetExitStatusFromCompile(int& exitcode);
  string CopyAndLoadNewPlugin(bool redirect_stdout);

 private:
  // global state
  std::vector<std::pair<string, void*>> plugins_;
  std::filesystem::file_time_type out_err_file_last_modification_time_;
  std::string compiler_output_;
  std::future<int> future_;
  bool last_compile_successful_ = false;
  PluginParser parser_;
};

}  // namespace rcrl
