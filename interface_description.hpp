// interface_description.hpp

#pragma once

#include <QStringList>
#include <QVariant>

struct method_struct {
  enum class type { pure, realized };
  type type;
  QString name;
  QString signature;
};

struct interface_description {
  /**\brief cmake package, where will be this interface*/
  QStringList packages;
  QString header;
  QString interface_class;
  QStringList inheritance_classes;
  std::list<method_struct> methods;
};

// for QVariant
Q_DECLARE_METATYPE(interface_description);
