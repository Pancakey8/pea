#pragma once
#include "ast.hpp"
#include "lexer.hpp"
#include <expected>

class Parser {
public:
  explicit Parser(Lexer &lexer);
  std::expected<Program, Error> parse();

private:
  Lexer &lexer;
  Token current;

  void advance();
  bool consume(TokenType type);

  std::expected<ExprPtr, Error> parse_expression(int precedence = 0);
  std::expected<ExprPtr, Error> parse_prefix();
  std::expected<ExprPtr, Error> parse_infix(ExprPtr left);

  std::expected<StmtPtr, Error> parse_statement();
  std::expected<std::vector<StmtPtr>, Error>
  parse_statements(TokenType stop_token = TokenType::EndOfFile);

  std::expected<StmtPtr, Error> parse_dim();
  std::expected<StmtPtr, Error> parse_let();
  std::expected<StmtPtr, Error> parse_if();
  std::expected<StmtPtr, Error> parse_for();
  std::expected<StmtPtr, Error> parse_goto();
  std::expected<StmtPtr, Error> parse_do();

  int get_precedence(TokenType type);
};
