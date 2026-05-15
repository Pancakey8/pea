#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <string>

int main() {
  std::ifstream file{"./test.pea"};
  std::stringstream file_ss{};
  file_ss << file.rdbuf();

  std::string input{file_ss.str()};
  std::println("Input: |{}|", input);

  {
    Lexer l{input};
    auto t = l.next_token();
    do {
      std::cout << *t << '\n';
    } while ((t = l.next_token()) && t->type != TokenType::EndOfFile);
  }

  Lexer lexer{input};
  Parser p{lexer};

  auto prog = p.parse();
  if (!prog) {
    std::println("Error {}:{}: {}", prog.error().line, prog.error().col,
                 prog.error().message);
    return 1;
  }

  std::cout << *prog;
}
