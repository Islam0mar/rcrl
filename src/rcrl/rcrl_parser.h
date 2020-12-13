#pragma once

#include <clang-c/Index.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace rcrl {
using std::string;

struct Point {
  unsigned int line;
  unsigned int column;
};

struct CodeBlock {
  Point start_pos;
  Point end_pos;
  CXCursor cursor;  // for any additional info.
};

inline string GetBaseNameFromSourceName(string str) {
  return str.substr(0, str.find('.'));
}
inline string GetHeaderNameFromSourceName(string str) {
  return str.substr(0, str.find('.')) + ".hpp";
}

class PluginParser {
 public:
  PluginParser(string file_name,
               std::vector<string> command_line_args = std::vector<string>(0));
  ~PluginParser();
  void Reparse();
  void GenerateSourceFile(string file_name, string prepend_str = "",
                          string append_str = "");
  void GenerateHeaderFile(string file_name);
  string get_file_name();
  std::vector<string> get_flags();
  // runs ParseWithOtherFlags internally
  void set_flags(std::vector<string> new_flags);

 private:
  void Parse();
  void UpdateAstWithOtherFlags();
  string ConsumeToLine(unsigned int line);
  string ReadToOneOfCharacters(Point start, string chars);
  void AppendRange(Point start, Point end);
  void AppendValidCodeBlockWithoutNamespace(CodeBlock code);
  void AppendValidCodeBlock(CodeBlock code);
  void AppendOnceCodeBlocks();

  string generated_file_content_;
  std::vector<string> file_content_;
  std::vector<CodeBlock> code_blocks_;
  std::vector<string> flags_;
  std::vector<std::tuple<Point, Point, string>> name_space_end_;
  std::tuple<CXIndex, CXTranslationUnit> ast_;
  const string file_name_;
  unsigned int code_gen_number_;
};

}  // namespace rcrl
