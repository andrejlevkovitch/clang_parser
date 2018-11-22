# cmake for interface_description_creator

cmake_minimum_required(VERSION "3.12")

project(interface_description_creator)

find_package(Qt5 COMPONENTS Core Widgets Xml XmlPatterns REQUIRED)
find_package(Clang REQUIRED)

set(IDC_SRC
  "clang_parser.cpp"
  )

add_executable(IDC main.cpp ${IDC_SRC})
target_link_libraries(IDC PUBLIC ${PROJECT_NAME} Qt5::Core Qt5::Widgets)

add_library(${PROJECT_NAME} ${IDC_SRC})
target_compile_options(${PROJECT_NAME} PRIVATE "-std=c++11")
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CLANG_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Core Qt5::Widgets Qt5::Xml Qt5::XmlPatterns libclang)
