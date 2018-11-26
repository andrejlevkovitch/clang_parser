// clang_parser.cpp

#include "clang_parser.hpp"
#include <QDomDocument>
#include <QFile>
#include <QTextStream>
#include <clang-c/Index.h>
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
/**\return name of file with string and column*/
QString get_spelling_string(const CXSourceLocation location);

::CXChildVisitResult general_visitor(::CXCursor cursor, ::CXCursor parent,
                                     ::CXClientData data);
::CXChildVisitResult class_visitor(::CXCursor cursor, ::CXCursor parent,
                                   ::CXClientData data);
::CXChildVisitResult template_visitor(::CXCursor cursor, ::CXCursor parent,
                                      ::CXClientData data);
// this function neded if we want found inheritance template, from other
// template
::CXChildVisitResult find_inheritance_template(::CXCursor cursor,
                                               ::CXCursor parent,
                                               ::CXClientData data);

::CXChildVisitResult param_visitor(::CXCursor cursor, ::CXCursor parent,
                                   ::CXClientData data);

// this class automatic free memory of translation unit and index
struct lock_ast {
  lock_ast() : index{clang_createIndex(0, 0)} {};
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
  includes.resize(include_directories.size() + 2);
  const char **args = new const char *[includes.size()]();
  for (int i{}; i < include_directories.size(); ++i) {
    includes[i] = "-I" + include_directories[i].toStdString();
    args[i] = includes[i].c_str();
  }
  // set c++ compiler
  includes[includes.size() - 2] = "-x";
  includes[includes.size() - 1] = "c++";
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

  // here we get all warnings and errors while compile
  auto root = ::clang_getTranslationUnitCursor(locker.unit);
  for (unsigned i{}; i < ::clang_getNumDiagnostics(locker.unit); ++i) {
    CXDiagnostic diagnostic = ::clang_getDiagnostic(locker.unit, i);

    // get type of diagnostic (warning, error, ...)
    switch (::clang_getDiagnosticSeverity(diagnostic)) {
    case CXDiagnostic_Fatal:
    case CXDiagnostic_Error: {
      auto location = ::clang_getDiagnosticLocation(diagnostic);

      auto output_str = ::clang_getDiagnosticSpelling(diagnostic);
      QString error =
          get_spelling_string(location) + '\n' + clang_getCString(output_str);
      ::clang_disposeString(output_str);

      ::clang_disposeDiagnostic(diagnostic);
      throw std::runtime_error{error.toStdString()};
      break;
    }
    default:
      ::clang_disposeDiagnostic(diagnostic);
      break;
    }
  }

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
  // only if it is file, which we set for compile, we parse it
  if (::clang_Location_isFromMainFile(::clang_getCursorLocation(cursor))) {
    switch (::clang_getCursorKind(cursor)) {
    case ::CXCursor_Namespace: {
      ::clang_visitChildren(cursor, general_visitor, data);
    } break;
    case ::CXCursor_ClassDecl:
    case ::CXCursor_StructDecl: {
      auto interfase_list =
          reinterpret_cast<std::list<interface_description> *>(data);

      ::QString full_class_name = get_spelling_string(
          ::clang_getCursorType(::clang_getCursorDefinition(cursor)));

      // if it is predeclared class
      if (full_class_name.isEmpty()) {
        break;
      }

      interfase_list->push_back(interfase_list->front());
      // set full name of class
      interfase_list->back().interface_class = full_class_name;

      ::clang_visitChildren(cursor, class_visitor, data);

      // if class is empty, then remove it from list
      if (interfase_list->back().inheritance_classes.isEmpty() &&
          interfase_list->back().methods.empty()) {
        auto for_erase = interfase_list->end();
        interfase_list->erase(--for_erase);
      }
    } break;
    // if this is template, then we can not get definition of it
    case ::CXCursor_ClassTemplate: {
      auto interfase_list =
          reinterpret_cast<std::list<interface_description> *>(data);

      // here we find full name of parsing class
      CXCursor temp = ::clang_getCursorSemanticParent(cursor);
      QStringList full_class_name;
      full_class_name.push_front(get_spelling_string(cursor));
      while (::clang_getCursorKind(temp) != CXCursor_TranslationUnit) {
        full_class_name.push_front(get_spelling_string(temp));
        temp = ::clang_getCursorSemanticParent(temp);
        if (::clang_getCursorKind(temp) == ::CXCursor_FirstInvalid) {
          break;
        }
      }

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

      // if class is empty, then remove it from list
      if (interfase_list->back().inheritance_classes.isEmpty() &&
          interfase_list->back().methods.empty()) {
        auto for_erase = interfase_list->end();
        interfase_list->erase(--for_erase);
      }
    } break;
    default:
      break;
    }
  }
  return ::CXChildVisit_Continue;
}

::CXChildVisitResult class_visitor(::CXCursor cursor, ::CXCursor parent,
                                   ::CXClientData data) {
  switch (::clang_getCursorKind(cursor)) {
  case ::CXCursor_Constructor: {
    method_struct method;
    method.type = method_struct::type::realized;
    method.name = get_spelling_string(cursor);

    QStringList params;
    ::clang_visitChildren(cursor, param_visitor, &params);
    if (!params.isEmpty()) {
      method.signature = '(' + params.join(" ,") + ')';
    } else {
      method.signature = "()";
    }

    auto interfase_list =
        reinterpret_cast<std::list<interface_description> *>(data);
    interfase_list->back().methods.push_back(method);
  } break;
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

    QStringList params;
    ::clang_visitChildren(cursor, param_visitor, &params);
    if (!params.isEmpty()) {
      method.signature =
          method.signature.section('(', 0, 0) + '(' + params.join(" ,") + ')';
    }

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

    // if main class is temlate, which inheritance template, and set for him
    // template parameter, then we can not get instance of this class. So for
    // full place, I get them string of this class as and search it in the tree
    if (inheritace_class.isEmpty()) {
      CXCursor temp = ::clang_getCursorSemanticParent(parent);
      while (::clang_getCursorKind(temp) != CXCursor_TranslationUnit) {
        temp = ::clang_getCursorSemanticParent(temp);
        if (::clang_getCursorKind(temp) == ::CXCursor_FirstInvalid) {
          break;
        }
      }

      inheritace_class =
          get_spelling_string(cursor).section('<', 0, 0).section("::", -1);
      ::clang_visitChildren(temp, find_inheritance_template, &inheritace_class);
    }

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

::CXChildVisitResult find_inheritance_template(::CXCursor cursor,
                                               ::CXCursor parent,
                                               ::CXClientData data) {
  switch (::clang_getCursorKind(cursor)) {
  case ::CXCursor_Namespace: {
    // if we in system header, then we don't visit childrens
    if (!::clang_Location_isInSystemHeader(::clang_getCursorLocation(cursor))) {
      ::clang_visitChildren(cursor, find_inheritance_template, data);
    }
  } break;
  case ::CXCursor_ClassTemplate: {
    // if we in system header, then we don't visit childrens
    if (!::clang_Location_isInSystemHeader(::clang_getCursorLocation(cursor))) {
      auto class_name = reinterpret_cast<QString *>(data);

      // if it is searched class, we return this
      if (class_name == get_spelling_string(cursor)) {
        CXCursor temp = ::clang_getCursorSemanticParent(cursor);
        // we have to get name with all namespaces
        QStringList full_class_name{*class_name};
        while (::clang_getCursorKind(temp) != CXCursor_TranslationUnit) {
          full_class_name.push_front(get_spelling_string(temp));
          temp = ::clang_getCursorSemanticParent(temp);
        }
        ::QStringList templates;
        ::clang_visitChildren(cursor, template_visitor, &templates);
        full_class_name.last() += '<' + templates.join(',') + '>';

        *class_name = full_class_name.join("::");
      }

      ::clang_visitChildren(cursor, find_inheritance_template, data);
    }
  } break;
  default:
    break;
  }
  return ::CXChildVisit_Continue;
}

::CXChildVisitResult param_visitor(::CXCursor cursor, ::CXCursor parent,
                                   ::CXClientData data) {
  if (::clang_getCursorKind(cursor) == ::CXCursor_ParmDecl) {
    CXType type = ::clang_getCursorType(cursor);
    QString param = get_spelling_string(type);

    param += ' ' + get_spelling_string(cursor);
    // we store all arguments in list, and take it for template class
    static_cast<QStringList *>(data)->append(param);
  }
  return ::CXChildVisit_Continue;
}

QString get_spelling_string(const CXSourceLocation location) {
  QString retval;
  ::CXFile file{};
  unsigned line{};
  unsigned column{};
  ::clang_getSpellingLocation(location, &file, &line, &column, nullptr);
  auto str = ::clang_getFileName(file);
  retval = QString{::clang_getCString(str)} + ':' + QString::number(line) +
           ':' + QString::number(column);
  ::clang_disposeString(str);
  return retval;
}
