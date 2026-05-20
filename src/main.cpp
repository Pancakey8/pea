#include "ast.hpp"
#include "bytecode.hpp"
#include "irgen.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <string>

int main() {
  std::ifstream file{ "./test.pea" };
  std::stringstream file_ss{};
  file_ss << file.rdbuf();

  std::string input{ file_ss.str() };
  std::println("Input: |{}|", input);

  {
    Lexer l{ input };
    auto t = l.next_token();
    do {
      std::cout << *t << '\n';
    } while ((t = l.next_token()) && t->type != TokenType::EndOfFile);
  }

  Lexer lexer{ input };
  Parser p{ lexer };

  auto prog = p.parse();
  if (!prog) {
    std::println("Error {}:{} to {}:{}: {}",
      prog.error().range.start.line,
      prog.error().range.start.col,
      prog.error().range.end.line,
      prog.error().range.end.col,
      prog.error().message);
    return 1;
  }

  std::cout << *prog << '\n';

  IrGen gen{};
  auto ir = gen.generate(*prog);
  if (!ir) {
    std::println("Error {}:{} to {}:{}: {}",
      ir.error().range.start.line,
      ir.error().range.start.col,
      ir.error().range.end.line,
      ir.error().range.end.col,
      ir.error().message);
    return 1;
  }

  std::cout << *ir << '\n';

  BytecodeEmitter emitter{ *ir };
  std::vector<std::uint8_t> bytecode{};
  emitter.emit(bytecode);

  for (std::size_t base = 0; base < bytecode.size(); base += 8) {
    for (std::size_t col = 0; col < 8; ++col) {
      if (base + col >= bytecode.size())
        std::print("00 ");
      else {
        std::print("{:02X} ", bytecode[base + col]);
      }
    }
    std::print("| ");
    for (std::size_t col = 0; col < 8; ++col) {
      if (base + col >= bytecode.size())
        std::print(". ");
      else if (bytecode[base + col] >= 32 && bytecode[base + col] <= 127)
        std::print("{:c} ", bytecode[base + col]);
      else std::print(". ");
    }
    std::print("\n");
  }
}
