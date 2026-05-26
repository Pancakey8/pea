#pragma once
#include <expected>
#include <string>
#include <string_view>
#include <vector>
#include <ostream>

struct SourceLocation {
  int line;
  int col;
};

struct SourceRange {
  SourceLocation start;
  SourceLocation end;
};

struct Error {
  std::string message;
  SourceRange range;
};

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
  KW_Mod,
  KW_And,
  KW_Or,
  KW_Not,
  KW_Sub,
  KW_EndSub,
  KW_Return,
  KW_Break,
  KW_Continue,
  KW_ByRef,
  KW_ByVal,
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
  Dot,
  // Special
  EOL,
  EndOfFile
};

struct Token {
  TokenType type;
  std::string text;
  double number = 0.0;
  char character = 0;
  SourceLocation start;
  SourceLocation end;
};

std::string to_string(TokenType type);
std::ostream& operator<<(std::ostream& os, const Token& token);

class Lexer {
public:
  explicit Lexer(std::string_view source);
  std::expected<Token, Error> next_token();

private:
  std::string_view source;
  size_t pos = 0;
  int line = 1;
  int col = 1;

  void skip_whitespace();
  std::expected<Token, Error> read_number();
  std::expected<Token, Error> read_string();
  std::expected<Token, Error> read_char();
  std::expected<Token, Error> read_ident_or_keyword();
  std::expected<Token, Error> read_operator();
};

std::ostream& operator<<(std::ostream& os, const Token& token);
