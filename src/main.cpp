#include "ast.hpp"
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
}
