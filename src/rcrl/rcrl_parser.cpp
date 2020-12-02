#include "rcrl_parser.h"

#include <clang-c/Index.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "config.h"
#include "debug.hpp"

namespace fs = std::filesystem;

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
bool operator==(const Point& p1, const Point& p2) { return p1.line == p2.line; }
bool operator<(const Point& p1, const Point& p2) {
  return p1.line < p2.line || (p1.line == p2.line && p1.column < p2.column);
}
std::ostream& operator<<(std::ostream& out, const Point& p) {
  return out << p.line << ":" << p.column;
}
std::ostream& operator<<(std::ostream& out, const CodeBlock& p) {
  return out << p.start_pos.line << ":" << p.start_pos.column;
}

constexpr auto kCursorInvalidCodeBlock = CXCursor_ObjCNSObject;
static inline bool IsOnceCodeBlock(CXCursor c) {
  return c.kind == kCursorInvalidCodeBlock;
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
      code.start_pos.column = col;
      code.start_pos.line = lin;
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
  for (unsigned I = 0, N = clang_getNumDiagnostics(ast); I != N; ++I) {
    CXDiagnostic diag = clang_getDiagnostic(ast, I);
    // clang_diagnostic
    // TODO: error: C++ requires a type specifier for all declarations
    unsigned int lin, prev_lin = 0;
    CXFile f;
    CodeBlock code;
    auto loc = clang_getDiagnosticLocation(diag);
    clang_getExpansionLocation(loc, &f, &lin, nullptr, nullptr);
    if (lin != prev_lin &&
        std::find_if(code_blocks_ptr->begin(), code_blocks_ptr->end(),
                     [&](const CodeBlock c) {
                       return lin == c.start_pos.line;
                     }) == code_blocks_ptr->end()) {
      code.start_pos.column = 1;
      code.start_pos.line = lin;
      code.cursor = clang_getCursor(ast, loc);
      code.cursor.kind = kCursorInvalidCodeBlock;
      code_blocks_ptr->emplace_back(code);
      auto str = clang_getDiagnosticSpelling(diag);
      DEBUG(lin, clang_getCString(str));
      clang_disposeString(str);
    }
    prev_lin = lin;
  }
}

void CreateCmakeListsFile(string file_name, std::vector<const char*> flags_v) {
  // generate Tupfile
  string flags;
  for (auto arg : flags_v) {
    flags += arg;
    flags += " ";
  }
  std::ofstream f("CMakeLists.txt", std::fstream::out | std::fstream::trunc);
  f << "cmake_minimum_required(VERSION 3.16)\n"
    << "set(CMAKE_CXX_COMPILER clang++)\n"
    << "project(" << GetBaseNameFromSourceName(file_name) << " CXX)\n"
    << "set(CMAKE_VERBOSE_MAKEFILE ON)\n"
    << "add_library(${PROJECT_NAME}  SHARED " << file_name << " "
    << GetHeaderNameFromSourceName(file_name) << ")\n"
    << "target_compile_options(" << GetBaseNameFromSourceName(file_name)
    << " PUBLIC -fvisibility=hidden " << flags << ")\n"
    << "add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD "
       "COMMAND cp compound.o old.o\n"
       "COMMAND ld -r ./CMakeFiles/plugin.dir/${PROJECT_NAME}.cpp.o old.o -o "
       "compound.o\n"
       "COMMAND cp compound.o old.o\n"
       "COMMAND cp compound.o ./CMakeFiles/plugin.dir/${PROJECT_NAME}.cpp.o)\n";
  // "add_dependencies(${PROJECT_NAME} copy)\n";
  f.close();
  std::cout << system("echo \"\">compound.o");
  std::cout << system("cmake -GNinja  .");
}

void PluginParser::Parse() {
  std::ifstream file(file_name_, std::fstream::in);
  file_content_.str(std::string());
  name_space_end_.clear();
  file_content_ << file.rdbuf();
  code_blocks_.clear();
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
  // generate Tupfile
  CreateCmakeListsFile(file_name_, flags_);
}

void PluginParser::ParseWithOtherFlags() {
  std::ifstream file(file_name_, std::fstream::in);
  file_content_.str(std::string());
  name_space_end_.clear();
  file_content_ << file.rdbuf();
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
  CreateCmakeListsFile(file_name_, flags_);
}

void PluginParser::Reparse() {
  std::ifstream file(file_name_, std::fstream::in);
  file_content_.str(std::string());
  name_space_end_.clear();
  file_content_ << file.rdbuf();
  code_blocks_.clear();
  auto ast = std::get<1>(ast_);
  clang_reparseTranslationUnit(ast, 0, 0, CXReparse_None);
  GenerateCodeBlocksFromAst(ast, &code_blocks_);
}

PluginParser::PluginParser(string file_name, std::vector<const char*> flags)
    : str_(""), flags_(flags), file_name_(file_name), code_gen_number_(0) {
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

string PluginParser::ConsumeToLine(unsigned int line) {
  unsigned int x = 0;
  std::istringstream ss;
  ss.str(file_content_.str());
  string s;
  while (line > x && std::getline(ss, s)) {
    x++;
  }
  return s;
}
string PluginParser::ReadToOneOfCharacters(Point start, string chars) {
  unsigned int x = 0;
  std::istringstream ss;
  ss.str(file_content_.str());
  string str;
  if (!(start == Point{0, 0})) {
    while (start.line > x++ && std::getline(ss, str)) {
    }
    char c;
    str.push_back('\n');
    auto i = std::find_if(str.begin(), str.end(), [&](const char c) {
      return chars.find(c) != chars.npos;
    });
    if (i != str.end()) {
      str = str.substr(start.column - 1, i - str.begin() - start.column + 1);
    } else {
      while (ss.get(c)) {
        if (chars.find(c) != chars.npos) {
          break;
        }
        str.push_back(c);
      }
    }
  }
  return str;
}
void PluginParser::AppendRange(Point start, Point end) {
  auto line = start.line;
  auto str = ConsumeToLine(line);
  if (start < end) {
    if (line == end.line) {
      if (end.column > 0) {
        str_.append(
            str.substr(start.column - 1, end.column - start.column + 1));
        str_.push_back('\n');
      }
    } else {
      str_.append(str.substr(start.column - 1));
      str_.push_back('\n');
      for (++line; line < end.line; line++) {
        str_.append(ConsumeToLine(line));
        str_.push_back('\n');
      }
      str = ConsumeToLine(end.line);
      str_.append(str.substr(0, end.column + 1));
      str_.push_back('\n');
    }
  }
}

void PluginParser::AppendCodeBlockWithoutNamespace(CodeBlock code) {
  switch (clang_getCursorKind(code.cursor)) {
    case CXCursor_Namespace: {
      auto str = ReadToOneOfCharacters(code.start_pos, "{") + "{\n";
      name_space_end_.emplace_back(
          std::make_tuple(code.start_pos, code.end_pos, str));
      break;
    }
    case CXCursor_InclusionDirective: {
      str_.append(ReadToOneOfCharacters(code.start_pos, "\n") + "\n");
      break;
    }
    default: {
      // once declarations
      if (IsOnceCodeBlock(code.cursor)) {
        str_.append(ReadToOneOfCharacters(code.start_pos, ";") + ";\n");
      } else {
        // global and variable declarations
        AppendRange(code.start_pos, code.end_pos);
      }
      break;
    }
  }
}
void PluginParser::AppendValidCodeBlock(CodeBlock code) {
  int i = 0;
  for (const auto& [start, end, str] : name_space_end_) {
    if (start < code.start_pos && code.end_pos < end) {
      str_.append(str);
      i++;
    }
  }
  AppendCodeBlockWithoutNamespace(code);
  for (int j = 0; j < i; ++j) {
    str_.append("}\n");
  }
}

void PluginParser::AppendOnceCodeBlock(CodeBlock code) {
  int i = 0;
  // once can't be repeated in the same line
  for (const auto& [start, end, str] : name_space_end_) {
    if (start < code.start_pos && code.end_pos < end) {
      str_.append(str);
      i++;
    }
  }
  AppendCodeBlockWithoutNamespace(code);
  for (int j = 0; j < i; ++j) {
    str_.append("}\n");
  }
}

void PluginParser::GenerateSourceFile(string file_name, string prepend_str,
                                      string append_str) {
  std::vector<unsigned int> lines;
  DEBUG(code_blocks_, 1);
  str_ = "";
  str_ += prepend_str;

  for (const auto& code : code_blocks_) {
    if (!IsOnceCodeBlock(code.cursor)) {
      auto kind = clang_getCursorKind(code.cursor);
      if (kind == CXCursor_UsingDirective ||
          kind == CXCursor_FunctionTemplate ||
          kind == CXCursor_InclusionDirective) {
        AppendValidCodeBlock(code);
      } else {
        str_ += __STR(RCRL_EXPORT_API) + string(" ");
        auto i = str_.size();
        AppendValidCodeBlock(code);
        if (str_.find(" auto ", i) < str_.find("{", i)) {
          CXString c_str = clang_getTypeSpelling(
              clang_getResultType(clang_getCursorType(code.cursor)));
          string return_type = clang_getCString(c_str);
          clang_disposeString(c_str);
          str_.replace(str_.find(" auto"), 5, return_type);
        }
      }
    }
  }
  str_ += "int __rcrl_internal_once_" + std::to_string(code_gen_number_++) +
          " = [](){\n";
  for (const auto& code : code_blocks_) {
    if (std::find(lines.begin(), lines.end(), code.start_pos.line) ==
        lines.end()) {
      if (IsOnceCodeBlock(code.cursor)) {
        auto str = ReadToOneOfCharacters(code.start_pos, ";") + ";\n";
        str_.append(str);
      }
    }
    lines.emplace_back(code.start_pos.line);
  }
  str_ += "  return 0;}();\n";
  str_ += append_str;
  auto old_str = str_;
  CacheHeaderFile();
  std::ofstream file(file_name, std::fstream::out | std::fstream::trunc);
  file << old_str;
}

void PluginParser::CacheHeaderFile() {
  str_ = "";
  for (const auto& code : code_blocks_) {
    switch (clang_getCursorKind(code.cursor)) {
      case CXCursor_FunctionTemplate:
      case CXCursor_UsingDirective: {
        AppendValidCodeBlock(code);
        break;
      }
      case CXCursor_InclusionDirective: {
        // avoid including the same header
        auto str = ReadToOneOfCharacters(code.start_pos, "\n") + "\n";
        auto header = GetHeaderNameFromSourceName(file_name_);
        if (str.find(header) == std::string::npos) {
          str_.append(str);
        }
        break;
      }
      default: {
        if (!IsOnceCodeBlock(code.cursor)) {
          // add space to void matching auto substring
          auto str = __STR(RCRL_IMPORT_API) + string(" ") +
                     ReadToOneOfCharacters(code.start_pos, "{=(") + ";\n";
          if (str.find(" auto ") != std::string::npos) {
            CXString c_str =
                clang_getTypeSpelling(clang_getCursorType(code.cursor));
            string variable_type = clang_getCString(c_str);
            clang_disposeString(c_str);
            auto gen_sym = "_" + std::to_string(code_gen_number_++) + "_t";
            variable_type = string("using ") + gen_sym + string(" = ") +
                            variable_type + string(";\nextern " + gen_sym);
            str.replace(str.find(" auto"), 5, variable_type);
          } else {
            str = "extern " + str;
          }
          str_.append(str);
          break;
        }
      }
    }
  }
}

void PluginParser::GenerateHeaderFile(string file_name) {
  std::ofstream file(file_name, std::fstream::out | std::fstream::app);
  file << str_;
}

}  // namespace rcrl
