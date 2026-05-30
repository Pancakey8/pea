#include "ast.hpp"
#include "bytecode.hpp"
#include "irgen.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "vm.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
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

int main() {
  std::println("Pea BASIC Interactive Shell");

  std::string collected{};
  char const *prompt_normal = "Pea> ";
  char const *prompt_continue = "...> ";

  while (true) {
    char *raw_input = readline(collected.empty() ? prompt_normal : prompt_continue);

    if (!raw_input)
      break;

    std::string input{std::move(collected)};
    input += std::string{raw_input};
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
      break;
    }

    collected = std::string{};

    IrGen irgen{};
    auto ir = irgen.generate(*prog);
    if (!ir) {
      print_error(ir.error());
      break;
    }

    BytecodeEmitter emitter{*ir};
    std::vector<std::uint8_t> bytes{};
    emitter.emit(bytes);

    Vm vm{bytes};
    vm.run();
  }
}
