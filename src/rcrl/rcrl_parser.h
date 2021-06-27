#pragma once

#include <clang-c/Index.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

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

class PluginParser {
 public:
  PluginParser(fs::path file,
               std::vector<string> command_line_args = std::vector<string>(0));
  ~PluginParser();
  void Reparse();
  void GenerateSourceFile(string file_name, string prepend_str = "",
                          string append_str = "");
  void GenerateHeaderFile(string file_name);
  fs::path get_file();
  std::vector<string> get_flags();
  // runs UpdateAstWithOtherFlags internally
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
  const fs::path file_path_;
  unsigned int code_gen_number_;
};

}  // namespace rcrl
