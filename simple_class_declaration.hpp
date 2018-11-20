// simple_class_declaration.hpp

#pragma once

class simple {
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
