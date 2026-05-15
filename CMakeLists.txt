cmake_minimum_required(VERSION 4.0)
project(pea)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(pea-lexer src/lexer.cpp)
target_include_directories(pea-lexer PUBLIC src/)

add_library(pea-parser src/parser.cpp src/ast_print.cpp)
target_include_directories(pea-parser PUBLIC src/)
target_link_libraries(pea-parser pea-lexer)

add_executable(pea src/main.cpp)
target_include_directories(pea PUBLIC src/)
target_link_libraries(pea pea-lexer pea-parser)

add_executable(pea-test-parser test/parser/test_runner.cpp)
target_link_libraries(pea-test-parser pea-lexer pea-parser)
