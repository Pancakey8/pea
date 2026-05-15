#pragma once
#include "ast.hpp"
#include "lexer.hpp"
#include <expected>

class Parser {
public:
    explicit Parser(Lexer& lexer);
    std::expected<Program, std::string> parse();

private:
    Lexer& lexer;
    Token current;

    void advance();
    std::expected<void, std::string> consume(TokenType type, std::string_view msg);
    
    std::expected<ExprPtr, std::string> parse_expression(int precedence = 0);
    std::expected<ExprPtr, std::string> parse_prefix();
    std::expected<ExprPtr, std::string> parse_infix(ExprPtr left);
    
    std::expected<StmtPtr, std::string> parse_statement();
    std::expected<std::vector<StmtPtr>, std::string> parse_statements(TokenType stop_token = TokenType::EndOfFile);
    
    std::expected<StmtPtr, std::string> parse_dim();
    std::expected<StmtPtr, std::string> parse_let();
    std::expected<StmtPtr, std::string> parse_if();
    std::expected<StmtPtr, std::string> parse_for();
    std::expected<StmtPtr, std::string> parse_goto();
    std::expected<StmtPtr, std::string> parse_do();

    int get_precedence(TokenType type);
};
