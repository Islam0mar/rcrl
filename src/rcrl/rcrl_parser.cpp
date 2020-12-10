#include "rcrl_parser.h"

#include <clang-c/Index.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "config.h"
#include "debug.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using VS = std::vector<const char*>;

namespace rcrl {
std::ostream& operator<<(std::ostream& stream, const CXString& str) {
  stream << clang_getCString(str);
  clang_disposeString(str);
  return stream;
}
bool operator<(const Point& p1, const Point& p2) {
  return p1.line < p2.line || (p1.line == p2.line && p1.column < p2.column);
}
std::ostream& operator<<(std::ostream& out, const Point& p) {
  return out << p.line << ":" << p.column;
}
std::ostream& operator<<(std::ostream& out, const CodeBlock& p) {
  return out << p.start_pos.line << ":" << p.start_pos.column;
}

auto AstVisitor(CXCursor c, CXCursor parent, CXClientData code_blocks_ptr) {
  CXFile f;
  auto return_val = CXChildVisit_Continue;
  auto& code_blocks =
      *reinterpret_cast<std::vector<CodeBlock>*>(code_blocks_ptr);
  clang_getExpansionLocation(clang_getCursorLocation(c), &f, nullptr, nullptr,
                             nullptr);
  // parse file when:
  //                  in main file
  if (clang_Location_isFromMainFile(clang_getCursorLocation(c)) != 0) {
    unsigned int lin, col;
    CodeBlock code;
    if (clang_getCursorKind(c) == CXCursor_InclusionDirective) {
      clang_getExpansionLocation(clang_getCursorLocation(c), nullptr, &lin,
                                 &col, nullptr);
      clang_getExpansionLocation(clang_getRangeStart(clang_getCursorExtent(c)),
                                 nullptr, &lin, &col, nullptr);
      code.start_pos.column = col;
      code.start_pos.line = lin;
      clang_getExpansionLocation(clang_getRangeEnd(clang_getCursorExtent(c)),
                                 nullptr, &lin, &col, nullptr);
      code.end_pos.column = col;
      code.end_pos.line = lin;
      code.cursor = c;
      code_blocks.emplace_back(code);
    } else if (clang_isDeclaration(clang_getCursorKind(c)) != 0 &&
               clang_isInvalidDeclaration(c) == 0) {
      // global section
      if (clang_getCursorKind(c) != CXCursor_VarDecl) {
        if (clang_getCursorKind(c) == CXCursor_Namespace) {
          return_val = CXChildVisit_Recurse;
        }
        clang_getExpansionLocation(
            clang_getRangeStart(clang_getCursorExtent(c)), nullptr, &lin, &col,
            nullptr);
        code.start_pos.column = col;
        code.start_pos.line = lin;
        clang_getExpansionLocation(clang_getRangeEnd(clang_getCursorExtent(c)),
                                   nullptr, &lin, &col, nullptr);
        code.end_pos.column = col;
        code.end_pos.line = lin;
        code.cursor = c;
      } else {
        // var section
        clang_getExpansionLocation(
            clang_getRangeStart(clang_getCursorExtent(c)), nullptr, &lin, &col,
            nullptr);
        code.start_pos.column = col;
        code.start_pos.line = lin;
        clang_getExpansionLocation(clang_getRangeEnd(clang_getCursorExtent(c)),
                                   nullptr, &lin, &col, nullptr);
        code.end_pos.column = col;
        code.end_pos.line = lin;
        code.cursor = c;
      }
      code_blocks.emplace_back(code);
    }
  }
  return return_val;
}

void GenerateCodeBlocksFromAst(CXTranslationUnit ast,
                               std::vector<CodeBlock>* code_blocks_ptr) {
  CXCursor cursor = clang_getTranslationUnitCursor(ast);
  clang_visitChildren(cursor, AstVisitor, code_blocks_ptr);
}

void PluginParser::Parse() {
  std::ifstream file(file_name_, std::fstream::in);
  string line;
  file_content_.clear();
  while (std::getline(file, line)) {
    file_content_.emplace_back(line + "\n");
  }
  file.close();
  code_blocks_.clear();
  name_space_end_.clear();
  CXIndex index = clang_createIndex(0, 0);
  CXTranslationUnit ast = clang_parseTranslationUnit(
      index, file_name_.c_str(), flags_.data(), flags_.size(), nullptr, 0,
      CXTranslationUnit_DetailedPreprocessingRecord |  // make headers readable
          CXTranslationUnit_Incomplete | CXTranslationUnit_KeepGoing |
          CXTranslationUnit_CreatePreambleOnFirstParse |
          CXTranslationUnit_IgnoreNonErrorsFromIncludedFiles |
          CXTranslationUnit_IncludeAttributedTypes);
  GenerateCodeBlocksFromAst(ast, &code_blocks_);
  ast_ = std::make_tuple(index, ast);
}

void PluginParser::ParseWithOtherFlags() {
  std::ifstream file(file_name_, std::fstream::in);
  string line;
  file_content_.clear();
  while (std::getline(file, line)) {
    file_content_.emplace_back(line + "\n");
  }
  file.close();
  name_space_end_.clear();
  code_blocks_.clear();
  auto [i, tu] = ast_;
  clang_disposeTranslationUnit(tu);
  CXTranslationUnit ast = clang_parseTranslationUnit(
      i, file_name_.c_str(), flags_.data(), flags_.size(), nullptr, 0,
      CXTranslationUnit_DetailedPreprocessingRecord |  // make headers readable
          CXTranslationUnit_Incomplete | CXTranslationUnit_KeepGoing |
          CXTranslationUnit_CreatePreambleOnFirstParse |
          CXTranslationUnit_IgnoreNonErrorsFromIncludedFiles |
          CXTranslationUnit_IncludeAttributedTypes);
  GenerateCodeBlocksFromAst(ast, &code_blocks_);
  ast_ = std::make_tuple(i, ast);
}

void PluginParser::Reparse() {
  std::ifstream file(file_name_, std::fstream::in);
  string line;
  file_content_.clear();
  while (std::getline(file, line)) {
    file_content_.emplace_back(line + "\n");
  }
  file.close();
  name_space_end_.clear();
  code_blocks_.clear();
  auto ast = std::get<1>(ast_);
  clang_reparseTranslationUnit(ast, 0, 0, CXReparse_None);
  GenerateCodeBlocksFromAst(ast, &code_blocks_);
}

PluginParser::PluginParser(string file_name, std::vector<const char*> flags)
    : generated_file_content_(""),
      flags_(flags),
      file_name_(file_name),
      code_gen_number_(0) {
  // create empty file
  std::ofstream file(file_name_, std::fstream::out | std::fstream::trunc);
  file << "\n";
  file.close();
  Parse();
}

PluginParser::~PluginParser() {
  auto [i, ast] = ast_;
  clang_disposeTranslationUnit(ast);
  clang_disposeIndex(i);
}

string PluginParser::get_file_name() { return file_name_; }
std::vector<const char*> PluginParser::get_flags() { return flags_; }
void PluginParser::set_flags(std::vector<const char*> f) {
  flags_ = f;
  ParseWithOtherFlags();
}

string PluginParser::ReadToOneOfCharacters(Point start, string chars) {
  auto i = start.line - 1;
  auto j = start.column - 1;
  auto n = file_content_.size();
  auto result = string("");
  auto line = file_content_[i].substr(j);
  bool not_found = true;
  for (; not_found && i < n; j = 0, i++) {
    if (j == 0) {
      line = file_content_[i];
    } else {
      j = 0;
    }
    do {
      not_found = (chars.find(line[j++]) == chars.npos);
    } while (not_found && j < line.size());
    result += not_found && j > 0 ? line : line.substr(0, j - 1);
  }
  return result;
}
// end point shouldn't be taken
void PluginParser::AppendRange(Point start, Point end) {
  if (start < end) {
    DEBUG(file_content_[start.line - 1]);
    DEBUG(start, end);
    if (start.line == end.line) {
      generated_file_content_.append(file_content_[start.line - 1].substr(
          start.column - 1, end.column - start.column));
    } else {
      while (start.line < end.line) {
        generated_file_content_.append(file_content_[start.line - 1]);
        start.line++;
      }
      generated_file_content_.append(
          file_content_[end.line - 1].substr(0, end.column - 1));
    }
  }
}

void PluginParser::AppendValidCodeBlockWithoutNamespace(CodeBlock code) {
  switch (clang_getCursorKind(code.cursor)) {
    case CXCursor_Namespace: {
      auto str = ReadToOneOfCharacters(code.start_pos, "{") + "{\n";
      name_space_end_.emplace_back(
          std::make_tuple(code.start_pos, code.end_pos, str));
      break;
    }
    default: {
      // global and variable declarations
      AppendRange(code.start_pos, code.end_pos);
      break;
    }
  }
  if (clang_getCursorKind(code.cursor) != CXCursor_InclusionDirective) {
    generated_file_content_.append(";\n");
  } else {
    generated_file_content_.append("\n");
  }
}

void PluginParser::AppendValidCodeBlock(CodeBlock code) {
  int i = 0;
  for (const auto& [start, end, str] : name_space_end_) {
    if (start < code.start_pos && code.end_pos < end) {
      generated_file_content_.append(str);
      i++;
    }
  }
  AppendValidCodeBlockWithoutNamespace(code);
  for (int j = 0; j < i; ++j) {
    generated_file_content_.append("}\n");
  }
}

void PluginParser::AppendOnceCodeBlocks() {
  std::sort(code_blocks_.begin(), code_blocks_.end(),
            [](const CodeBlock& a, const CodeBlock& b) {
              return a.start_pos < b.start_pos;
            });
  Point p = {1, 1};
  auto line = p.line;
  auto column = p.column;
  // append every unparsed piece of text to once function.
  for (auto c : code_blocks_) {
    while (line < c.start_pos.line) {
      generated_file_content_.append(
          file_content_[line - 1].substr(column - 1));
      line++;
      column = 1;
    }
    generated_file_content_.append(file_content_[line - 1].substr(
        column - 1, c.start_pos.column - column));
    if (clang_getCursorKind(c.cursor) != CXCursor_Namespace) {
      line = c.end_pos.line;
      column = c.end_pos.column;
    } else {
      // skip to '{'
      auto namespace_begin = ReadToOneOfCharacters(c.start_pos, "{") + "{";
      line += std::count(namespace_begin.begin(), namespace_begin.end(), '\n');
      column = file_content_[line].find("{") + 1;
      generated_file_content_.append(namespace_begin + "\n");
      // closing '}' will be appended by the above procedure
    }
  }
  while (line < file_content_.size()) {
    generated_file_content_.append(file_content_[line - 1].substr(column - 1));
    line++;
    column = 1;
  }
  generated_file_content_.append(file_content_[line - 1].substr(column - 1));
}

void PluginParser::GenerateSourceFile(string file_name, string prepend_str,
                                      string append_str) {
  std::vector<unsigned int> lines;
  generated_file_content_ = "";
  generated_file_content_ += prepend_str;

  for (const auto& code : code_blocks_) {
    auto kind = clang_getCursorKind(code.cursor);
    if (kind == CXCursor_UsingDirective || kind == CXCursor_FunctionTemplate ||
        kind == CXCursor_InclusionDirective) {
      AppendValidCodeBlock(code);
    } else {
      // variable/functions declarations
      // TODO: extract function parameter intializers.
      generated_file_content_ += __STR(RCRL_EXPORT_API) + string(" ");
      auto i = generated_file_content_.size() - 1;
      AppendValidCodeBlock(code);
      auto j = generated_file_content_.find(" auto ", i);
      if (j != std::string::npos && j < generated_file_content_.find("{", i)) {
        CXString c_str;
        if (clang_getCursorKind(code.cursor) == CXCursor_VarDecl) {
          c_str = clang_getTypeSpelling((clang_getCursorType(code.cursor)));
        } else {
          // func decl
          c_str = clang_getTypeSpelling(
              clang_getResultType(clang_getCursorType(code.cursor)));
        }
        string return_type = clang_getCString(c_str);
        clang_disposeString(c_str);
        generated_file_content_.replace(
            generated_file_content_.find(" auto", i), 5, return_type);
      }
    }
  }
  generated_file_content_ += "\nint __rcrl_internal_once_" +
                             std::to_string(code_gen_number_++) + " = [](){\n";
  AppendOnceCodeBlocks();
  generated_file_content_ += "  return 0;}();\n";
  generated_file_content_ += append_str;
  std::ofstream file(file_name, std::fstream::out | std::fstream::trunc);
  file << generated_file_content_;
}

void PluginParser::GenerateHeaderFile(string file_name) {
  generated_file_content_ = "";
  DEBUG(code_blocks_);
  for (const auto& code : code_blocks_) {
    switch (clang_getCursorKind(code.cursor)) {
      case CXCursor_Namespace: {
        break;
      }
      case CXCursor_FunctionTemplate:
      case CXCursor_InclusionDirective:
      case CXCursor_UsingDirective: {
        AppendValidCodeBlock(code);
        break;
      }
      default: {
        CXString c_str;
        auto c = code.cursor;
        auto gen_sym = "_" + std::to_string(code_gen_number_++) + "_t";
        if (clang_getCursorKind(c) == CXCursor_VarDecl) {
          c_str = clang_getTypeSpelling((clang_getCursorType(c)));
          generated_file_content_ += string("using ") + gen_sym +
                                     string(" = ") + clang_getCString(c_str) +
                                     string(";\n" RCRL_IMPORT_API " extern ") +
                                     gen_sym + " ";
          clang_disposeString(c_str);
          c_str = clang_getCursorSpelling(c);
          generated_file_content_ += clang_getCString(c_str);
        } else {
          // func decl
          // using x = return_type;
          // extern return_type ...
          c_str = clang_getTypeSpelling(
              clang_getResultType(clang_getCursorType(c)));
          generated_file_content_ += string("using ") + gen_sym +
                                     string(" = ") + clang_getCString(c_str) +
                                     string(";\n" RCRL_IMPORT_API " extern ") +
                                     gen_sym + " ";
          clang_disposeString(c_str);
          // extern return_type f_name(...
          c_str = clang_getCursorSpelling(c);
          generated_file_content_ += clang_getCString(c_str) + string("(");
          // extern return_type f_name(arg1 , arg2, ...)
          for (auto i = 0, n = clang_Cursor_getNumArguments(c); i < n; ++i) {
            if (i > 0) {
              generated_file_content_ += ", ";
            }
            auto c_arg = clang_Cursor_getArgument(c, i);
            unsigned int lin, col;
            c_arg = clang_getCursorDefinition(c_arg);
            clang_getExpansionLocation(
                clang_getRangeStart(clang_getCursorExtent(c_arg)), nullptr,
                &lin, &col, nullptr);
            Point start = {lin, col};
            clang_getExpansionLocation(
                clang_getRangeEnd(clang_getCursorExtent(c_arg)), nullptr, &lin,
                &col, nullptr);
            Point end = {lin, col};
            DEBUG(start, end);
            AppendRange(start, end);
          }
          if (clang_Cursor_isVariadic(c)) {
            generated_file_content_ += "...";
          }
          generated_file_content_ += ")";
        }
        clang_disposeString(c_str);
        generated_file_content_ += ";\n";
        break;
      }
    }
  }
  std::ofstream file(file_name, std::fstream::out | std::fstream::app);
  file << generated_file_content_;
}

}  // namespace rcrl
