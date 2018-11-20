// main.cpp

#include "clang_parser.hpp"
#include <QApplication>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc > 1) {
    //  ::QCoreApplication app(argc, argv);

    clang_parser clang_parser{argv[1]};

    std::cout << "exit\n";

    //  return app.exec();
    return EXIT_SUCCESS;
  }

  return EXIT_FAILURE;
}
