// clang_parser.hpp

#pragma once

#include "interface_data.hpp"
#include <QString>
#include <QStringList>
#include <clang-c/Index.h>

class clang_parser {
public:
  /**\param file_name header file with path to it
   * \param include_directories paths to headers, included in parsing file
   * */
  clang_parser(const QString &file_name,
               const QStringList &include_directories = QStringList{});
  ~clang_parser();

  const interface_data &get_interface_data() const;

private:
  QString file_name_;
  CXIndex index_;
  CXTranslationUnit unit_;
  interface_data data_;
};
