// simple_class_declaration.hpp

#pragma once

#include "simple_class_template.hpp"

namespace general {
namespace my_namespace {
class simple : simple_class_template<int, float> {
public:
  simple(int alfa);
  void realized_method();
  void cont_realized_method() const;
  virtual void virtual_method();
  virtual void pure_virtual_method() = 0;
  void with_param(int alfa);

private:
  int get_int();

private:
  int variable_;
};
};
};
