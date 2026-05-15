#include "lexer.hpp"
#include <algorithm>
#include <cctype>
#include <unordered_map>

Lexer::Lexer(std::string_view src) : source(src) {}

void Lexer::skip_whitespace() {
  while (pos < source.size() && std::isspace(source[pos])) {
    if (source[pos] == '\n') {
      line++;
      col = 1;
    } else {
      col++;
    }
    pos++;
  }
}

std::expected<Token, std::string> Lexer::next_token() {
  skip_whitespace();
  if (pos >= source.size())
    return Token{TokenType::EndOfFile, "", 0, 0, line, col};

  char c = source[pos];
  int start_col = col;

  if (c == '\n') {
    pos++;
    line++;
    col = 1;
    return Token{TokenType::EOL, "\n", 0, 0, line - 1, start_col};
  }

  if (std::isdigit(c) || ((c == '+' || c == '-') && pos + 1 < source.size() &&
                          std::isdigit(source[pos + 1])))
    return read_number();
  if (c == '"')
    return read_string();
  if (c == '\'')
    return read_char();
  if (std::isalpha(c) || c == '_')
    return read_ident_or_keyword();

  return read_operator();
}

std::expected<Token, std::string> Lexer::read_number() {
  size_t start = pos;
  int start_col = col;
  if (source[pos] == '+' || source[pos] == '-') {
    pos++;
    col++;
  }
  while (pos < source.size() && std::isdigit(source[pos])) {
    pos++;
    col++;
  }
  if (pos < source.size() && source[pos] == '.') {
    pos++;
    col++;
    while (pos < source.size() && std::isdigit(source[pos])) {
      pos++;
      col++;
    }
  }
  std::string text{source.substr(start, pos - start)};
  return Token{TokenType::Number, text, std::stod(text), 0, line, start_col};
}

std::expected<Token, std::string> Lexer::read_string() {
  int start_col = col;
  pos++;
  col++; // skip "
  std::string result;
  while (pos < source.size() && source[pos] != '"') {
    if (source[pos] == '\\') {
      pos++;
      col++;
      if (pos >= source.size())
        return std::unexpected("Unterminated string escape");
      char esc = source[pos];
      if (esc == 'n')
        result += '\n';
      else if (esc == '\\')
        result += '\\';
      else if (esc == '"')
        result += '"';
      else
        result += esc;
    } else {
      result += source[pos];
    }
    pos++;
    col++;
  }
  if (pos >= source.size())
    return std::unexpected("Unterminated string");
  pos++;
  col++; // skip "
  return Token{TokenType::String, result, 0, 0, line, start_col};
}

std::expected<Token, std::string> Lexer::read_char() {
  int start_col = col;
  pos++;
  col++; // skip '
  char result;
  if (pos < source.size() && source[pos] == '\\') {
    pos++;
    col++;
    if (pos >= source.size())
      return std::unexpected("Unterminated char escape");
    char esc = source[pos];
    if (esc == 'n')
      result = '\n';
    else if (esc == '\\')
      result = '\\';
    else if (esc == '\'')
      result = '\'';
    else
      result = esc;
  } else if (pos < source.size()) {
    result = source[pos];
  } else
    return std::unexpected("Unterminated char");
  pos++;
  col++;
  if (pos >= source.size() || source[pos] != '\'')
    return std::unexpected("Missing closing quote for char");
  pos++;
  col++;
  return Token{TokenType::Char, std::string(1, result), 0, result, line,
               start_col};
}

std::expected<Token, std::string> Lexer::read_ident_or_keyword() {
  size_t start = pos;
  int start_col = col;
  while (pos < source.size() &&
         (std::isalnum(source[pos]) || source[pos] == '_')) {
    pos++;
    col++;
  }
  std::string text{source.substr(start, pos - start)};
  std::string lower = text;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  static const std::unordered_map<std::string, TokenType> keywords = {
      {"dim", TokenType::KW_Dim},     {"let", TokenType::KW_Let},
      {"if", TokenType::KW_If},       {"then", TokenType::KW_Then},
      {"else", TokenType::KW_Else},   {"for", TokenType::KW_For},
      {"to", TokenType::KW_To},       {"step", TokenType::KW_Step},
      {"while", TokenType::KW_While}, {"do", TokenType::KW_Do},
      {"loop", TokenType::KW_Loop},   {"goto", TokenType::KW_Goto},
      {"end", TokenType::KW_End},     {"as", TokenType::KW_As},
      {"mod", TokenType::KW_Mod},     {"and", TokenType::KW_And},
      {"or", TokenType::KW_Or},       {"not", TokenType::KW_Not}};

  if (auto it = keywords.find(lower); it != keywords.end()) {
    // Special handling for "end if" and "end for"
    if (it->second == TokenType::KW_End) {
      size_t save_pos = pos;
      int save_col = col;
      skip_whitespace();
      if (pos + 2 <= source.size() && source.substr(pos, 2) == "if") {
        pos += 2;
        col += 2;
        return Token{TokenType::KW_EndIf, "end if", 0, 0, line, start_col};
      } else if (pos + 3 <= source.size() && source.substr(pos, 3) == "for") {
        pos += 3;
        col += 3;
        return Token{TokenType::KW_EndFor, "end for", 0, 0, line, start_col};
      }
      pos = save_pos;
      col = save_col;
    }
    return Token{it->second, text, 0, 0, line, start_col};
  }
  return Token{TokenType::Ident, text, 0, 0, line, start_col};
}

std::expected<Token, std::string> Lexer::read_operator() {
  int start_col = col;
  char c = source[pos];
  pos++;
  col++;
  if (c == '+')
    return Token{TokenType::Plus, "+", 0, 0, line, start_col};
  if (c == '-')
    return Token{TokenType::Minus, "-", 0, 0, line, start_col};
  if (c == '*')
    return Token{TokenType::Star, "*", 0, 0, line, start_col};
  if (c == '/')
    return Token{TokenType::Slash, "/", 0, 0, line, start_col};
  if (c == '^')
    return Token{TokenType::Caret, "^", 0, 0, line, start_col};
  if (c == '\\')
    return Token{TokenType::Backslash, "\\", 0, 0, line, start_col};
  if (c == '(')
    return Token{TokenType::LParen, "(", 0, 0, line, start_col};
  if (c == ')')
    return Token{TokenType::RParen, ")", 0, 0, line, start_col};
  if (c == ',')
    return Token{TokenType::Comma, ",", 0, 0, line, start_col};
  if (c == ':')
    return Token{TokenType::Colon, ":", 0, 0, line, start_col};
  if (c == '&')
    return Token{TokenType::Amp, "&", 0, 0, line, start_col};
  if (c == '=')
    return Token{TokenType::Eq, "=", 0, 0, line, start_col};
  if (c == '<') {
    if (pos < source.size() && source[pos] == '>') {
      pos++;
      col++;
      return Token{TokenType::Neq, "<>", 0, 0, line, start_col};
    }
    if (pos < source.size() && source[pos] == '=') {
      pos++;
      col++;
      return Token{TokenType::Lteq, "<=", 0, 0, line, start_col};
    }
    return Token{TokenType::Less, "<", 0, 0, line, start_col};
  }
  if (c == '>') {
    if (pos < source.size() && source[pos] == '=') {
      pos++;
      col++;
      return Token{TokenType::Gteq, ">=", 0, 0, line, start_col};
    }
    return Token{TokenType::Great, ">", 0, 0, line, start_col};
  }
  return std::unexpected(std::string("Unexpected character: ") + c);
}
