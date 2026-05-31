#include "ast.hpp"
#include "bytecode.hpp"
#include "irgen.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "vm.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <print>
#include <readline/history.h>
#include <readline/readline.h>
#include <sstream>
#include <string>

void print_error(Error const &err) {
  std::println("Error {}:{} -> {}:{}: {}",
    err.range.start.line,
    err.range.start.col,
    err.range.end.line,
    err.range.end.col,
    err.message);
}

int main(int argc, char **argv) {
  if (argc >= 2) {
    std::ifstream file{argv[1]};
    if (!file) {
      std::println("File not found: {}", argv[1]);
    }
    std::stringstream buf{};
    buf << file.rdbuf();
    std::string input{buf.str()};
    file.close();
    Lexer lexer{input};
    Parser parser{lexer};
    auto prog = parser.parse();
    if (!prog) {
      print_error(prog.error());
      return 1;
    }
    IrGen irgen{};
    auto ir = irgen.generate(*prog);
    if (!ir) {
      print_error(ir.error());
      return 1;      
    }
    std::vector<std::uint8_t> bytecode{};
    BytecodeEmitter emitter{};
    emitter.emit(*ir, bytecode);
    auto vm = std::make_unique<Vm>();
    vm->append(bytecode);
    vm->run();
    return 0;
  }

  std::println("Pea BASIC Interactive Shell");

  std::string collected{};
  char const *prompt_normal = "Pea> ";
  char const *prompt_continue = "...> ";

  IrGen irgen{};
  BytecodeEmitter byteEmitter{};
  auto vm = std::make_unique<Vm>();

  vm->on_error([](Error error) {
    std::println("VM Exception {}:{} to {}:{}: {}",
      error.range.start.line,
      error.range.start.col,
      error.range.end.line,
      error.range.end.col,
      error.message);
  });

  while (true) {
    char *raw_input =
      readline(collected.empty() ? prompt_normal : prompt_continue);

    if (!raw_input)
      break;

    std::string input{ std::move(collected) };
    input += std::string{ raw_input };
    std::free(raw_input);

    if (input.empty())
      break;

    add_history(input.c_str());

    input += '\n';

    Lexer lexer{ std::move(input) };
    Parser parser{ lexer };
    auto prog = parser.parse();
    if (!prog) {
      if (parser.is_continuing()) {
        collected = std::move(input);
        continue;
      }
      print_error(prog.error());
      continue;
    }

    collected = std::string{};

    auto ir = irgen.generate(*prog);
    if (!ir) {
      print_error(ir.error());
      continue;
    }

    // std::cout << *ir << "\n";

    std::vector<std::uint8_t> bytecode{};
    byteEmitter.emit(*ir, bytecode);

    // for (std::size_t base = 0; base < bytecode.size(); base += 8) {
    //   for (std::size_t col = 0; col < 8; ++col) {
    // 	if (base + col >= bytecode.size())
    //       std::print("00 ");
    // 	else {
    //       std::print("{:02X} ", bytecode[base + col]);
    // 	}
    //   }
    //   std::print("| ");
    //   for (std::size_t col = 0; col < 8; ++col) {
    // 	if (base + col >= bytecode.size())
    //       std::print(". ");
    // 	else if (bytecode[base + col] >= 32 && bytecode[base + col] <= 127)
    //       std::print("{:c} ", bytecode[base + col]);
    // 	else std::print(". ");
    //   }
    //   std::print("\n");
    // }

    vm->append(bytecode);
    vm->run();
  }
}
