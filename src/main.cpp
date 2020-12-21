#include <readline/history.h>
#include <readline/readline.h>

#include <chrono>
#include <cmath>
#include <string>

#include "rcrl/debug.hpp"
#include "rcrl/rcrl.h"

using std::string;

static std::vector<std::string> vocabulary{"std::",     "std::string",
                                           "std::cout", "std::cin",
                                           "std::endl", "std::vector"};

// This function is called with state=0 the first time; subsequent calls are
// with a nonzero state. state=0 can be used to perform one-time
// initialization for this completion session.
extern "C" char *my_generator(const char *text, int state) {
  static std::vector<std::string> matches;
  static size_t match_index = 0;

  if (state == 0) {
    // During initialization, compute the actual matches for 'text' and keep
    // them in a static vector.
    matches.clear();
    match_index = 0;

    // Collect a vector of matches: vocabulary words that begin with text.
    std::string textstr = std::string(text);
    for (auto word : vocabulary) {
      if (word.size() >= textstr.size() &&
          word.compare(0, textstr.size(), textstr) == 0) {
        matches.push_back(word);
      }
    }
  }

  if (match_index >= matches.size()) {
    // We return nullptr to notify the caller no more matches are available.
    return nullptr;
  } else {
    // Return a malloc'd char* for the match. The caller frees it.
    return strdup(matches[match_index++].c_str());
  }
}

// Custom completion function
extern "C" char **my_completion(const char *text, int start, int end) {
  rl_completer_quote_characters = "\"'";
  // Don't do filename completion even if our generator finds no matches.
  rl_attempted_completion_over = 1;

  // Note: returning nullptr here will make readline use the default filename
  // completer.
  return rl_completion_matches(text, my_generator);
}

int main() {
  string line;
  bool done = false;

  // enable auto-complete
  rl_bind_key('\t', rl_complete);
  rl_attempted_completion_function = my_completion;

  std::vector<string> args = {"-std=c++17", "-O0", "-Wall", "-Wextra",
                              "-ggdb3"};
  rcrl::Plugin compiler("plugin", args);

  while (!done) {
    line = readline("> ");
    line += "\n";
    if (line.compare(".q\n") == 0) {
      done = true;
    } else if (line.compare(".clean\n") == 0) {
      auto output_from_cleanup = compiler.CleanupPlugins();
      std::cout << output_from_cleanup;
    } else if (line.substr(0, 6).compare(".flags") == 0) {
      line = line.substr(6);
      size_t start;
      size_t end = 0;
      args.clear();
      while ((start = line.find_first_not_of(' ', end)) != std::string::npos) {
        end = line.find(' ', start);
        args.emplace_back(line.substr(start, end - start));
      }
      compiler.set_flags(args);
    } else if (line.substr(0, 2).compare(".f") == 0) {
      line = line.substr(2);
      size_t start;
      size_t end = 0;
      args.clear();
      while ((start = line.find_first_not_of(' ', end)) != std::string::npos) {
        end = line.find(' ', start);
        args.emplace_back(line.substr(start, end - start));
      }
      compiler.set_flags(args);
    } else if (!line.empty()) {
      compiler.CompileCode(line);
      int last_compiler_exitcode;
      while (!compiler.TryGetExitStatusFromCompile(last_compiler_exitcode)) {
        std::cout << compiler.get_new_compiler_output();
      }
      // drop signal line
      compiler.get_new_compiler_output();
      // append to the history
      add_history(line.c_str());

      if (last_compiler_exitcode == 0) {
        // load the new plugin
        compiler.CopyAndLoadNewPlugin();
      }
    }
  }

  return 0;
}
