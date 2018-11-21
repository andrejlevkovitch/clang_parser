// clang_parser.cpp

#include "clang_parser.hpp"
#include <QStringList>
#include <clang-c/Index.h>
#include <iostream>
#include <stdexcept>

QString get_spelling_string(const CXCursor cursor) {
  QString retval;
  CXString clang_cx_str = ::clang_getCursorSpelling(cursor);
  retval = ::clang_getCString(clang_cx_str);
  ::clang_disposeString(clang_cx_str);
  return retval;
}

QString get_spelling_string(const CXType type) {
  QString retval;
  CXString clang_cx_str = ::clang_getTypeSpelling(type);
  retval = ::clang_getCString(clang_cx_str);
  ::clang_disposeString(clang_cx_str);
  return retval;
}

QString get_spelling_string(const CXFile file) {
  QString retval;
  CXString clang_cx_str = ::clang_getFileName(file);
  retval = ::clang_getCString(clang_cx_str);
  ::clang_disposeString(clang_cx_str);
  return retval;
}

::CXChildVisitResult general_visitor(::CXCursor cursor, ::CXCursor parent,
                                     ::CXClientData data);
::CXChildVisitResult class_visitor(::CXCursor cursor, ::CXCursor parent,
                                   ::CXClientData data);
::CXChildVisitResult template_visitor(::CXCursor cursor, ::CXCursor parent,
                                      ::CXClientData data);

clang_parser::clang_parser(const QString &file_name,
                           const QStringList &include_directories)
    // TODO turn of display diagnostic (param two in clang_createIndex)
    : file_name_{file_name},
      index_{clang_createIndex(0, 1)} {

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

  std::list<interface_data> list_of_interfaces;

  ::QStringList namespaces_list{};

  // we set namespaces_list as data, becose we wont to now full class names
  auto root = ::clang_getTranslationUnitCursor(unit_);
  ::clang_visitChildren(root, general_visitor, &namespaces_list);
}

clang_parser::~clang_parser() {
  ::clang_disposeTranslationUnit(unit_);
  ::clang_disposeIndex(index_);
}

const interface_data &clang_parser::get_interface_data() const { return data_; }

::CXChildVisitResult general_visitor(::CXCursor cursor, ::CXCursor parent,
                                     ::CXClientData data) {
  switch (::clang_getCursorKind(cursor)) {
  case ::CXCursor_Namespace: {
    // get name of namespace and put it in data
    QString cursor_spelling = get_spelling_string(cursor);
    static_cast<QStringList *>(data)->append(cursor_spelling);

    ::clang_visitChildren(cursor, general_visitor, data);

    static_cast<QStringList *>(data)->removeLast();
  } break;
  case ::CXCursor_ClassDecl:
  case ::CXCursor_ClassTemplate: {
    ::CXSourceLocation location = ::clang_getCursorLocation(cursor);
    ::CXFile file;
    ::clang_getFileLocation(location, &file, nullptr, nullptr, nullptr);
    QString file_name = get_spelling_string(file);
    // here we have to check input file, because clang parse all includes files
    //  if (file_name != QString{"simple_class_declaration.hpp"}) {
    //    break;
    //  }

    QStringList *namespaces_list = static_cast<QStringList *>(data);

    QString only_class_name = get_spelling_string(cursor);
    namespaces_list->append(only_class_name);

    // if it is template we find all template parameters
    if (::clang_getCursorKind(cursor) == CXCursor_ClassTemplate) {
      ::QStringList templates;
      ::clang_visitChildren(cursor, template_visitor, &templates);
      namespaces_list->last() += '<' + templates.join(',') + '>';
    }

    QString full_class_name = namespaces_list->join("::");

    std::cout << "\nclass: " << full_class_name.toStdString() << std::endl;

    ::clang_visitChildren(cursor, class_visitor, data);

    namespaces_list->removeLast();
  } break;
  default:
    break;
  }
  return ::CXChildVisit_Continue;
}

::CXChildVisitResult class_visitor(::CXCursor cursor, ::CXCursor parent,
                                   ::CXClientData data) {
  switch (::clang_getCursorKind(cursor)) {
  case ::CXCursor_CXXMethod: {
    ::QString type = "pure";
    if (::clang_CXXMethod_isPureVirtual(cursor)) {
      type = "pure";
    } else {
      type = "realized";
    }

    ::QString method_name = get_spelling_string(cursor);

    auto cursor_type = ::clang_getCursorType(cursor);
    ::QString signature = get_spelling_string(cursor_type);

    std::cout << "  type: " << type.toStdString() << std::endl;
    std::cout << "  name: " << method_name.toStdString() << std::endl;
    std::cout << "  signature: " << signature.toStdString() << std::endl
              << std::endl;

  } break;
  case ::CXCursor_CXXBaseSpecifier: {
    CXType cursor_type =
        ::clang_getCursorType(::clang_getCursorDefinition(cursor));
    QString inheritace_class = get_spelling_string(cursor_type);

    std::cout << "  inheritance: " << inheritace_class.toStdString()
              << std::endl
              << std::endl;

  } break;
  default:
    break;
  }
  return ::CXChildVisit_Continue;
}

::CXChildVisitResult template_visitor(::CXCursor cursor, ::CXCursor parent,
                                      ::CXClientData data) {
  if (::clang_getCursorKind(cursor) == ::CXCursor_TemplateTypeParameter) {
    QString template_arg = get_spelling_string(cursor);
    // we store all arguments in list, and take it for template class
    static_cast<QStringList *>(data)->append(template_arg);
  }
  return ::CXChildVisit_Continue;
}
