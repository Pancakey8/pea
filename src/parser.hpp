#pragma once
#include "ast.hpp"
#include "lexer.hpp"
#include <expected>
#include <optional>

class Parser {
public:
  explicit Parser(Lexer &lexer);
  std::expected<Program, Error> parse();

private:
  Lexer &lexer;
  Token current;
  std::optional<Token> peeked_token;

  void advance();
  Token peek();
  bool consume(TokenType type);

  std::expected<ExprPtr, Error> parse_expression(int precedence = 0);
  std::expected<ExprPtr, Error> parse_prefix();
  std::expected<ExprPtr, Error> parse_infix(ExprPtr left);

  std::expected<StmtPtr, Error> parse_statement();
  std::expected<std::vector<StmtPtr>, Error>
  parse_statements(std::vector<TokenType> stop_tokens = {TokenType::EndOfFile});

  std::expected<StmtPtr, Error> parse_dim();
  std::expected<StmtPtr, Error> parse_let();
  std::expected<StmtPtr, Error> parse_if();
  std::expected<ElseBranch, Error> parse_else_branch();
  std::expected<StmtPtr, Error> parse_for();
  std::expected<StmtPtr, Error> parse_goto();
  std::expected<StmtPtr, Error> parse_do();

  int get_precedence(TokenType type);
};
