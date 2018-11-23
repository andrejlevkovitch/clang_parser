// main.cpp

#include "clang_parser.hpp"
#include <QApplication>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc > 2) {
    //  ::QCoreApplication app(argc, argv);

    // add included directories
    QStringList includes;
    for (int i{3}; i < argc; ++i) {
      includes << argv[i];
    }

    clang_parser clang_parser{};
    auto interfaces = clang_parser.create_description_from(argv[2], includes);

    for (auto &i : interfaces) {
      i.packages << "DS";
      clang_parser.generate_xml_file(i, QDir{argv[1]});
    }

    //  return app.exec();
    return EXIT_SUCCESS;
  }

  return EXIT_FAILURE;
}
