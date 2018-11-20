// clang_parser.cpp

#include "clang_parser.hpp"
#include <clang-c/Index.h>
#include <iostream>
#include <stdexcept>

::CXChildVisitResult visitor(::CXCursor cursor, ::CXCursor parent,
                             ::CXClientData data);

clang_parser::clang_parser(const QString &file_name,
                           const QStringList &include_directories)
    // TODO turn of display diagnostic (param two in clang_createIndex)
    : index_{clang_createIndex(0, 1)} {

  const std::string includes =
      include_directories.join(" -I").toStdString().c_str();
  const char *const args = includes.c_str();

  auto error = clang_parseTranslationUnit2(
      index_, file_name.toStdString().c_str(), &args, 1, nullptr, 0,
      CXTranslationUnit_None, &unit_);

  switch (error) {
  case CXError_Success:
    break;
  case CXError_Failure:
    throw std::runtime_error{"failure while reading file"};
    break;
  case CXError_Crashed:
    throw std::runtime_error{"crashed while reading file"};
    break;
  case CXError_InvalidArguments:
    throw std::runtime_error{"invalid argument"};
    break;
  case CXError_ASTReadError:
    throw std::runtime_error{"ast read error"};
    break;
  default:
    break;
  }

  auto root = ::clang_getTranslationUnitCursor(unit_);
  ::clang_visitChildren(root, visitor, nullptr);
}

clang_parser::~clang_parser() {
  ::clang_disposeTranslationUnit(unit_);
  ::clang_disposeIndex(index_);
}

const interface_data &clang_parser::get_interface_data() const { return data_; }

void print(CXCursor cursor) {
  CXString displayName = clang_getCursorDisplayName(cursor);
  std::cout << clang_getCString(displayName) << "\n";
  clang_disposeString(displayName);
}

::CXChildVisitResult visitor(CXCursor cursor, CXCursor parent,
                             CXClientData data) {
  auto cursor_kind = ::clang_getCursorKind(cursor);
  switch (cursor_kind) {
  case CXCursor_ClassDecl:
    print(cursor);
    ::clang_visitChildren(cursor, visitor, nullptr);
    break;
  case CXCursor_CXXMethod:
    print(cursor);
    break;
  default:
    return CXChildVisit_Continue;
    break;
  }
  return CXChildVisit_Continue;
}
