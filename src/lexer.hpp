#pragma once
#include <expected>
#include <string>
#include <string_view>
#include <vector>

enum class TokenType {
  // Literals
  Number,
  String,
  Char,
  Ident,
  // Keywords
  KW_Dim,
  KW_Let,
  KW_If,
  KW_Then,
  KW_Else,
  KW_EndIf,
  KW_For,
  KW_To,
  KW_Step,
  KW_EndFor,
  KW_While,
  KW_Do,
  KW_Loop,
  KW_Goto,
  KW_End,
  KW_As,
  KW_Mod,
  KW_And,
  KW_Or,
  KW_Not,
  // Operators
  Plus,
  Minus,
  Star,
  Slash,
  Caret,
  Backslash,
  Eq,
  Neq,
  Less,
  Great,
  Lteq,
  Gteq,
  Amp,
  LParen,
  RParen,
  Comma,
  Colon,
  // Special
  EOL,
  EndOfFile
};

struct Token {
  TokenType type;
  std::string text;
  double number = 0.0;
  char character = 0;
  int line = 1;
  int col = 1;
};

class Lexer {
public:
  explicit Lexer(std::string_view source);
  std::expected<Token, std::string> next_token();

private:
  std::string_view source;
  size_t pos = 0;
  int line = 1;
  int col = 1;

  void skip_whitespace();
  std::expected<Token, std::string> read_number();
  std::expected<Token, std::string> read_string();
  std::expected<Token, std::string> read_char();
  std::expected<Token, std::string> read_ident_or_keyword();
  std::expected<Token, std::string> read_operator();
};
