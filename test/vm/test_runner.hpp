#pragma once
#include "bytecode.hpp"
#include "irgen.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <print>
#define private public
#include "vm.hpp"
#undef private

std::unique_ptr<Vm> vm_run(std::string const &input);

extern std::size_t pass, total;

#define expect(n)                                                              \
  ++total;                                                                     \
  if (!(n)) {                                                                  \
    std::println("Test failed: {}", #n);                                       \
  } else {                                                                     \
    std::println("Test passed: {}", #n);                                       \
    ++pass;                                                                    \
  }

