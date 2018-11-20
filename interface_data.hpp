// interface_data.hpp

#pragma once

#include <QStringList>
#include <QVariant>

/**\brief this structure contains all neded information about class*/
struct interface_data {
  QStringList packages;
  QStringList headers;
  QStringList interface_class;
  QStringList inheritance_classes;
  QStringList abstract_methods;
  QStringList realized_methods;
};

// for QValidator
Q_DECLARE_METATYPE(interface_data);
