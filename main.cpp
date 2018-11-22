// main.cpp

#include "clang_parser.hpp"
#include <QApplication>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc > 1) {
    //  ::QCoreApplication app(argc, argv);

    // add included directories
    QStringList includes;
    for (int i{2}; i < argc; ++i) {
      includes << argv[i];
    }

    clang_parser clang_parser{};
    auto interfaces = clang_parser.create_description_from(argv[1], includes);

    for (const auto &i : interfaces) {
      clang_parser.generate_xml_file(
          i, QDir{"/home/levkovich/Public/temp/interface_description_creator/"
                  "xmls"});
    }

    //  return app.exec();
    return EXIT_SUCCESS;
  }

  return EXIT_FAILURE;
}
