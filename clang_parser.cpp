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

  // here we put all interface data. First node will be invalid, and intakes
  // only header file
  std::list<interface_data> list_of_interfaces;
  list_of_interfaces.push_back(interface_data{});
  list_of_interfaces.back().headers.append(file_name);

  // we set namespaces_list as data, becose we wont to now full class names
  auto root = ::clang_getTranslationUnitCursor(unit_);
  ::clang_visitChildren(root, general_visitor, &list_of_interfaces);

  // because first element is void
  list_of_interfaces.erase(std::begin(list_of_interfaces));

  for (auto &i : list_of_interfaces) {
    std::cout << "\nclass:\n";
    std::cout << "  " << i.interface_class.first().toStdString() << std::endl;

    if (!i.inheritance_classes.isEmpty()) {
      std::cout << "  inheritace:\n";
      for (auto &j : i.inheritance_classes) {
        std::cout << "    " << j.toStdString() << std::endl;
      }
    }

    if (!i.abstract_methods.isEmpty()) {
      std::cout << "  abs mehtods:\n";
      for (auto &j : i.abstract_methods) {
        std::cout << "    " << j.arg(' ').toStdString() << std::endl;
      }
    }

    if (!i.realized_methods.isEmpty()) {
      std::cout << "  realized mehtods:\n";
      for (auto &j : i.realized_methods) {
        std::cout << "    " << j.arg(' ').toStdString() << std::endl;
      }
    }
  }
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
    ::clang_visitChildren(cursor, general_visitor, data);
  } break;
  case ::CXCursor_ClassDecl:
  case ::CXCursor_ClassTemplate: {
    ::CXSourceLocation location = ::clang_getCursorLocation(cursor);
    ::CXFile file;
    ::clang_getFileLocation(location, &file, nullptr, nullptr, nullptr);
    QString file_name = get_spelling_string(file);
    // here we have to check input file, because clang parse all includes files
    // if (file_name != QString{"simple_class_declaration.hpp"}) {
    //  break;
    //}

    // here we find full name of parsing class
    CXCursor temp = cursor;
    QStringList full_class_name;
    do {
      full_class_name.push_front(get_spelling_string(temp));
      temp = ::clang_getCursorSemanticParent(temp);
    } while (::clang_getCursorKind(temp) != CXCursor_TranslationUnit);

    // if it is template we find all template parameters
    if (::clang_getCursorKind(cursor) == CXCursor_ClassTemplate) {
      ::QStringList templates;
      ::clang_visitChildren(cursor, template_visitor, &templates);
      full_class_name.last() += '<' + templates.join(',') + '>';
    }

    // QString full_class_name = namespaces_list->join("::");

    // std::cout << "\nclass: " << full_class_name.toStdString() << std::endl;

    auto interfase_list = reinterpret_cast<std::list<interface_data> *>(data);
    interfase_list->push_back(interfase_list->front());
    interfase_list->back().interface_class.push_back(
        full_class_name.join("::"));

    ::clang_visitChildren(cursor, class_visitor, data);

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
    auto interfase_list = reinterpret_cast<std::list<interface_data> *>(data);
    ::QString method_name = get_spelling_string(cursor);

    auto cursor_type = ::clang_getCursorType(cursor);
    ::QString signature = get_spelling_string(cursor_type);

    QString new_method =
        signature.insert(signature.indexOf('('), "%1" + method_name);

    if (::clang_CXXMethod_isPureVirtual(cursor)) {
      interfase_list->back().abstract_methods << new_method;
    } else {
      interfase_list->back().realized_methods << new_method;
    }
  } break;
  case ::CXCursor_CXXBaseSpecifier: {
    auto interfase_list = reinterpret_cast<std::list<interface_data> *>(data);
    CXType cursor_type =
        ::clang_getCursorType(::clang_getCursorDefinition(cursor));
    QString inheritace_class = get_spelling_string(cursor_type);

    interfase_list->back().inheritance_classes << inheritace_class;
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
