// clang_parser.cpp

#include "clang_parser.hpp"
#include <QDomDocument>
#include <QFile>
#include <QTextStream>
#include <clang-c/Index.h>
#include <iostream>
#include <stdexcept>

#define INTERF_SCHEMA ":/component_schema.xsd"
#define ROOT_NODE "description"
#define PACKAGES_NODES "packages"
#define PACKAGE_ITEM "package"
#define CLASS_NODE "class"
#define HEADER_NODE "header"
#define METHODS_NODES "methods"
#define METHOD_ITEM "method"
#define METHOD_TYPE "type"
#define METHOD_NAME "name"
#define METHOD_SIGNATURE "signature"
#define ABSTRACT_METHOD_TYPE "pure"
#define REALIZED_METHOD_TYPE "realized"
#define INHERITANCE_NODE "inheritance"

QString get_spelling_string(const CXCursor cursor);
QString get_spelling_string(const CXType type);
QString get_spelling_string(const CXFile file);

::CXChildVisitResult general_visitor(::CXCursor cursor, ::CXCursor parent,
                                     ::CXClientData data);
::CXChildVisitResult class_visitor(::CXCursor cursor, ::CXCursor parent,
                                   ::CXClientData data);
::CXChildVisitResult template_visitor(::CXCursor cursor, ::CXCursor parent,
                                      ::CXClientData data);

// this class automatic free memory of translation unit and index
struct lock_ast {
  lock_ast() : index{clang_createIndex(0, 1)} {};
  ~lock_ast() {
    ::clang_disposeTranslationUnit(unit);
    ::clang_disposeIndex(index);
  }
  CXIndex index;
  CXTranslationUnit unit;
};

clang_parser::clang_parser() {}

clang_parser::~clang_parser() {}

std::list<interface_description>
clang_parser::create_description_from(const QString &file_name,
                                      const QStringList &include_directories) {
  // return value
  std::list<interface_description> list_of_interfaces;

  // add includes directories for clang
  std::vector<std::string> includes;
  includes.resize(include_directories.size() + 3);
  const char **args = new const char *[includes.size()]();
  for (unsigned i{}; i < include_directories.size(); ++i) {
    includes[i] = "-I" + include_directories[i].toStdString();
    args[i] = includes[i].c_str();
  }
  // set c++ compiler
  includes[includes.size() - 3] = "-c";
  includes[includes.size() - 2] = "-x";
  includes[includes.size() - 1] = "c++";
  args[includes.size() - 3] = includes[includes.size() - 3].c_str();
  args[includes.size() - 2] = includes[includes.size() - 2].c_str();
  args[includes.size() - 1] = includes[includes.size() - 1].c_str();

  // create unit translation
  lock_ast locker;
  auto error = ::clang_parseTranslationUnit2(
      locker.index, file_name.toStdString().c_str(), args, includes.size(),
      nullptr, 0, CXTranslationUnit_None, &locker.unit);

  delete[] args;

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
    throw std::runtime_error{"unkhnow error"};
    break;
  }

  // here we put all interface data. First node will be invalid, and intakes
  // only header file
  list_of_interfaces.push_back(interface_description{});
  list_of_interfaces.back().header = file_name;

  auto root = ::clang_getTranslationUnitCursor(locker.unit);
  ::clang_visitChildren(root, general_visitor, &list_of_interfaces);

  // because first element is void
  list_of_interfaces.erase(std::begin(list_of_interfaces));

  return list_of_interfaces;
}

bool clang_parser::generate_xml_file(const interface_description &description,
                                     const QDir &dir) const {
  if (!dir.exists()) {
    std::string error{"directory: " + dir.absolutePath().toStdString() +
                      " not exists"};
    throw std::runtime_error{error};
  }
  ::QDomDocument xml_doc;
  auto instruction = xml_doc.createProcessingInstruction(
      "xml", "version=\"1.0\" encoding=\"UTF-8\"");
  xml_doc.appendChild(instruction);

  auto root = xml_doc.createElement(ROOT_NODE);
  xml_doc.appendChild(root);

  auto packages = xml_doc.createElement(PACKAGES_NODES);
  root.appendChild(packages);

  for (const auto &i : description.packages) {
    auto package = xml_doc.createElement(PACKAGE_ITEM);
    auto text = xml_doc.createTextNode(i);
    package.appendChild(text);
    packages.appendChild(package);
  }

  {
    auto header = xml_doc.createElement(HEADER_NODE);
    auto text_node = xml_doc.createTextNode(description.header);
    header.appendChild(text_node);
    root.appendChild(header);
  }

  {
    auto class_node = xml_doc.createElement(CLASS_NODE);
    auto text_node = xml_doc.createTextNode(description.interface_class);
    class_node.appendChild(text_node);
    root.appendChild(class_node);
  }

  {
    auto inheritance_nodes = xml_doc.createElement(INHERITANCE_NODE);
    root.appendChild(inheritance_nodes);
    for (const auto &i : description.inheritance_classes) {
      auto inheritace_class = xml_doc.createElement(CLASS_NODE);
      auto text_node = xml_doc.createTextNode(i);
      inheritace_class.appendChild(text_node);
      inheritance_nodes.appendChild(inheritace_class);
    }
  }

  {
    auto methods = xml_doc.createElement(METHODS_NODES);
    root.appendChild(methods);
    for (const auto &i : description.methods) {
      auto method_node = xml_doc.createElement(METHOD_ITEM);

      auto method_type = xml_doc.createElement(METHOD_TYPE);
      {
        QString type = (i.type == method_struct::type::pure)
                           ? ABSTRACT_METHOD_TYPE
                           : REALIZED_METHOD_TYPE;
        auto text_node = xml_doc.createTextNode(type);
        method_type.appendChild(text_node);
      }
      auto method_name = xml_doc.createElement(METHOD_NAME);
      {
        auto text_node = xml_doc.createTextNode(i.name);
        method_name.appendChild(text_node);
      }
      auto method_signature = xml_doc.createElement(METHOD_SIGNATURE);
      {
        auto text_node = xml_doc.createTextNode(i.signature);
        method_signature.appendChild(text_node);
      }
      method_node.appendChild(method_type);
      method_node.appendChild(method_name);
      method_node.appendChild(method_signature);

      methods.appendChild(method_node);
    }
  }

  // it is folder, when file will be set
  ::QDir destination = dir;
  for (int i{}; i < description.interface_class.count("::"); ++i) {
    QString name = description.interface_class.section("::", i, i);
    if (!QDir{destination.absolutePath() + QDir::separator() + name}.exists()) {
      if (!destination.mkdir(name)) {
        std::string error{"couldn't create new folder: " + name.toStdString() +
                          " in directory: " +
                          destination.absolutePath().toStdString()};
        throw std::runtime_error{error};
      }
    }
    destination = destination.absolutePath() + QDir::separator() + name;
  }

  // output file will be named as interface class withoud namespaces and
  // templates
  ::QFile file{
      destination.absolutePath() + QDir::separator() +
      description.interface_class.section('<', 0, 0).section("::", -1) +
      ".xml"};
  if (file.open(::QIODevice::WriteOnly | ::QIODevice::Text)) {
    ::QTextStream stream{&file};
    stream << xml_doc.toString();
    file.close();
    return true;
  }

  return false;
}

::CXChildVisitResult general_visitor(::CXCursor cursor, ::CXCursor parent,
                                     ::CXClientData data) {
  switch (::clang_getCursorKind(cursor)) {
  case ::CXCursor_Namespace: {
    ::clang_visitChildren(cursor, general_visitor, data);
  } break;
  case ::CXCursor_ClassDecl:
  case ::CXCursor_ClassTemplate:
  case ::CXCursor_StructDecl: {
    ::CXSourceLocation location = ::clang_getCursorLocation(cursor);
    ::CXFile file;
    ::clang_getFileLocation(location, &file, nullptr, nullptr, nullptr);
    QString file_name = get_spelling_string(file);
    // here we have to check input file, because clang parse all includes
    // files
    auto interfase_list =
        reinterpret_cast<std::list<interface_description> *>(data);
    if (file_name != interfase_list->front().header) {
      break;
    }

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

    // set full name of class
    interfase_list->push_back(interfase_list->front());
    interfase_list->back().interface_class = full_class_name.join("::");

    ::clang_visitChildren(cursor, class_visitor, data);

    // if this was be only predefinition of class we erase this
    if (interfase_list->back().inheritance_classes.isEmpty() &&
        interfase_list->back().methods.empty()) {
      auto for_erase = interfase_list->end();
      interfase_list->erase(--for_erase);
    }

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
    method_struct method;
    if (::clang_CXXMethod_isPureVirtual(cursor)) {
      method.type = method_struct::type::pure;
    } else {
      method.type = method_struct::type::realized;
    }

    method.name = get_spelling_string(cursor);

    CXType cursor_type = ::clang_getCursorType(cursor);
    method.signature = get_spelling_string(cursor_type);

    auto interfase_list =
        reinterpret_cast<std::list<interface_description> *>(data);
    interfase_list->back().methods.push_back(method);
  } break;
  case ::CXCursor_CXXBaseSpecifier: {
    auto interfase_list =
        reinterpret_cast<std::list<interface_description> *>(data);
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
