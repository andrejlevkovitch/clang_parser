// clang_parser.hpp

#pragma once

#include "interface_description.hpp"
#include <QDir>
#include <QString>
#include <QStringList>
#include <clang-c/Index.h>

class clang_parser {
public:
  /**\param file_name header file with path to it
   * \param include_directories paths to headers, included in parsing file
   * */
  clang_parser();
  ~clang_parser();

  /**\except if couldn't build correct ast tree
   * \return list of descriptions. If in file only one interface, then list will
   * have only one item
   * */
  std::list<interface_description> create_description_from(
      const QString &file_name,
      const QStringList &include_directories = QStringList{});

  /**\brief create xml file from interface_description in dir. If interface have
   * namespaces, then file will be in folder, with name of namespace
   * \return true if file was be generated, and false otherwise
   * \param dir folder where will be generated all neded files and folders(for
   * namespaces)
   * \except if dir not exists, or if couldn't create files or folders in this
   * dir
   * */
  bool generate_xml_file(const interface_description &description,
                         const QDir &dir) const;
};
