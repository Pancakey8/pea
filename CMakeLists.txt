cmake_minimum_required(VERSION 4.0)
project(pea)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(pea-lexer src/lexer.cpp)
target_include_directories(pea-lexer PUBLIC src/)

add_library(pea-parser src/parser.cpp src/ast_print.cpp)
target_include_directories(pea-parser PUBLIC src/)
target_link_libraries(pea-parser pea-lexer)

add_library(pea-irgen src/irgen.cpp)
target_include_directories(pea-irgen PUBLIC src/)
target_link_libraries(pea-irgen pea-parser pea-lexer)

add_library(pea-bytecode src/bytecode.cpp)
target_include_directories(pea-bytecode PUBLIC src/)
target_link_libraries(pea-bytecode pea-irgen)

add_library(pea-vm src/vm.cpp)
target_include_directories(pea-vm PUBLIC src/)
target_link_libraries(pea-vm pea-bytecode)

add_executable(pea src/main.cpp)
target_include_directories(pea PUBLIC src/)
target_link_libraries(pea pea-lexer pea-parser pea-irgen pea-bytecode pea-vm)

add_executable(pea-test-parser test/parser/test_runner.cpp)
target_link_libraries(pea-test-parser pea-lexer pea-parser)
