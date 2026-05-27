#include "lexer.hpp"
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <ostream>

Lexer::Lexer(std::string_view src) : source(src) {}

void Lexer::skip_whitespace() {
  while (pos < source.size() && std::isspace(source[pos]) && source[pos] != '\n') {
    col++;
    pos++;
  }
}

std::expected<Token, Error> Lexer::next_token() {
  skip_whitespace();
  if (pos >= source.size())
    return Token{TokenType::EndOfFile, "", 0, 0, {line, col}, {line, col}};

  char c = source[pos];
  int start_col = col;

  if (c == '\n') {
    pos++;
    line++;
    col = 1;
    return Token{TokenType::EOL, "\n", 0, 0, {line - 1, start_col}, {line - 1, start_col + 1}};
  }

  if (std::isdigit(c) || ((c == '+' || c == '-') && pos + 1 < source.size() &&
                          std::isdigit(source[pos + 1])))
    return read_number();
  if (c == '"')
    return read_string();
  if (c == '\'') {
    auto start_pos = pos;
    auto start_col = col;
    auto chr = read_char();
    if (!chr) {
      pos = start_pos;
      col = start_col;
      auto cmt = read_comment();
      if (!cmt) return chr;
      return next_token();
    }
    return chr;
  }
  if (std::isalpha(c) || c == '_')
    return read_ident_or_keyword();

  return read_operator();
}

std::expected<Token, Error> Lexer::read_number() {
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
  return Token{TokenType::Number, text, std::stod(text), 0, {line, start_col}, {line, col}};
}

std::expected<Token, Error> Lexer::read_string() {
  int start_col = col;
  pos++;
  col++; // skip "
  std::string result;
  while (pos < source.size() && source[pos] != '"') {
    if (source[pos] == '\\') {
      pos++;
      col++;
      if (pos >= source.size())
        return std::unexpected(Error{"Unterminated string escape", line, col});
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
    return std::unexpected(Error{"Unterminated string", line, col});
  pos++;
  col++; // skip "
  return Token{TokenType::String, result, 0, 0, {line, start_col}, {line, col}};
}

std::expected<void, Error> Lexer::read_comment() {
  int start_col = col;
  pos++;
  col++;

  while (pos < source.size() && source[pos] != '\n') {
    pos++;
    col++;
  }

  return {};
}

std::expected<Token, Error> Lexer::read_char() {
  int start_col = col;
  pos++;
  col++; // skip '
  char result;
  if (pos < source.size() && source[pos] == '\\') {
    pos++;
    col++;
    if (pos >= source.size())
      return std::unexpected(Error{"Unterminated char escape", line, col});
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
    return std::unexpected(Error{"Unterminated char", line, col});
  pos++;
  col++;
  if (pos >= source.size() || source[pos] != '\'')
    return std::unexpected(Error{"Missing closing quote for char", line, col});
  pos++;
  col++;
  return Token{TokenType::Char, std::string(1, result), 0, result, {line, start_col}, {line, col}};
}

std::expected<Token, Error> Lexer::read_ident_or_keyword() {
  size_t start = pos;
  int start_col = col;
  while (pos < source.size() &&
         (std::isalnum(source[pos]) || source[pos] == '_')) {
    pos++;
    col++;
  }
  std::string text{source.substr(start, pos - start)};
  auto to_lower = [](std::string_view str) {
    std::string lower{str};
    std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
    return lower;
  };
  std::string lower = to_lower(text);

  static const std::unordered_map<std::string, TokenType> keywords = {
      {"dim", TokenType::KW_Dim},     {"let", TokenType::KW_Let},
      {"if", TokenType::KW_If},       {"then", TokenType::KW_Then},
      {"else", TokenType::KW_Else},   {"for", TokenType::KW_For},
      {"to", TokenType::KW_To},       {"step", TokenType::KW_Step},
      {"while", TokenType::KW_While}, {"do", TokenType::KW_Do},
      {"loop", TokenType::KW_Loop},   {"goto", TokenType::KW_Goto},
      {"end", TokenType::KW_End},     {"byval", TokenType::KW_ByVal},
      {"mod", TokenType::KW_Mod},     {"and", TokenType::KW_And},
      {"or", TokenType::KW_Or},       {"not", TokenType::KW_Not},
      {"sub", TokenType::KW_Sub},     {"return", TokenType::KW_Return},
      {"break", TokenType::KW_Break}, {"continue", TokenType::KW_Continue},
      {"byref", TokenType::KW_ByRef}, {"class", TokenType::KW_Class},
      {"public", TokenType::KW_Public}, {"private", TokenType::KW_Private},
      {"static", TokenType::KW_Static}, {"new", TokenType::KW_New},};

  if (auto it = keywords.find(lower); it != keywords.end()) {
    // Special handling for "end if", "end for", and "end sub"
    if (it->second == TokenType::KW_End) {
      size_t save_pos = pos;
      int save_col = col;
      skip_whitespace();
      if (pos + 2 <= source.size() && to_lower(source.substr(pos, 2)) == "if") {
        pos += 2;
        col += 2;
        return Token{TokenType::KW_EndIf, "end if", 0, 0, {line, start_col}, {line, col}};
      } else if (pos + 3 <= source.size() && to_lower(source.substr(pos, 3)) == "for") {
        pos += 3;
        col += 3;
        return Token{TokenType::KW_EndFor, "end for", 0, 0, {line, start_col}, {line, col}};
      } else if (pos + 3 <= source.size() && to_lower(source.substr(pos, 3)) == "sub") {
        pos += 3;
        col += 3;
        return Token{TokenType::KW_EndSub, "end sub", 0, 0, {line, start_col}, {line, col}};
      } else if (pos + 5 <= source.size() && to_lower(source.substr(pos, 5)) == "class") {
        pos += 5;
        col += 5;
        return Token{TokenType::KW_EndClass, "end class", 0, 0, {line, start_col}, {line, col}};
      }
      pos = save_pos;
      col = save_col;
    }
    return Token{it->second, lower, 0, 0, {line, start_col}, {line, col}};
  }
  return Token{TokenType::Ident, lower, 0, 0, {line, start_col}, {line, col}};
}

std::expected<Token, Error> Lexer::read_operator() {
  int start_col = col;
  char c = source[pos];
  pos++;
  col++;
  if (c == '+')
    return Token{TokenType::Plus, "+", 0, 0, {line, start_col}, {line, col}};
  if (c == '-')
    return Token{TokenType::Minus, "-", 0, 0, {line, start_col}, {line, col}};
  if (c == '*')
    return Token{TokenType::Star, "*", 0, 0, {line, start_col}, {line, col}};
  if (c == '/')
    return Token{TokenType::Slash, "/", 0, 0, {line, start_col}, {line, col}};
  if (c == '^')
    return Token{TokenType::Caret, "^", 0, 0, {line, start_col}, {line, col}};
  if (c == '\\')
    return Token{TokenType::Backslash, "\\", 0, 0, {line, start_col}, {line, col}};
  if (c == '(')
    return Token{TokenType::LParen, "(", 0, 0, {line, start_col}, {line, col}};
  if (c == ')')
    return Token{TokenType::RParen, ")", 0, 0, {line, start_col}, {line, col}};
  if (c == ',')
    return Token{TokenType::Comma, ",", 0, 0, {line, start_col}, {line, col}};
  if (c == '.')
    return Token{TokenType::Dot, ".", 0, 0, {line, start_col}, {line, col}};
  if (c == ':')
    return Token{TokenType::Colon, ":", 0, 0, {line, start_col}, {line, col}};
  if (c == '&')
    return Token{TokenType::Amp, "&", 0, 0, {line, start_col}, {line, col}};
  if (c == '=')
    return Token{TokenType::Eq, "=", 0, 0, {line, start_col}, {line, col}};
  if (c == '<') {
    if (pos < source.size() && source[pos] == '>') {
      pos++;
      col++;
      return Token{TokenType::Neq, "<>", 0, 0, {line, start_col}, {line, col}};
    }
    if (pos < source.size() && source[pos] == '=') {
      pos++;
      col++;
      return Token{TokenType::Lteq, "<=", 0, 0, {line, start_col}, {line, col}};
    }
    return Token{TokenType::Less, "<", 0, 0, {line, start_col}, {line, col}};
  }
  if (c == '>') {
    if (pos < source.size() && source[pos] == '=') {
      pos++;
      col++;
      return Token{TokenType::Gteq, ">=", 0, 0, {line, start_col}, {line, col}};
    }
    return Token{TokenType::Great, ">", 0, 0, {line, start_col}, {line, col}};
  }
  return std::unexpected(Error{"Unexpected character: " + std::string(1, c), line, start_col});
}

std::string to_string(TokenType type) {
  switch (type) {
    case TokenType::Number: return "Number";
    case TokenType::String: return "String";
    case TokenType::Char: return "Char";
    case TokenType::Ident: return "Ident";
    case TokenType::KW_Dim: return "KW_Dim";
    case TokenType::KW_Let: return "KW_Let";
    case TokenType::KW_If: return "KW_If";
    case TokenType::KW_Then: return "KW_Then";
    case TokenType::KW_Else: return "KW_Else";
    case TokenType::KW_EndIf: return "KW_EndIf";
    case TokenType::KW_For: return "KW_For";
    case TokenType::KW_To: return "KW_To";
    case TokenType::KW_Step: return "KW_Step";
    case TokenType::KW_EndFor: return "KW_EndFor";
    case TokenType::KW_While: return "KW_While";
    case TokenType::KW_Do: return "KW_Do";
    case TokenType::KW_Loop: return "KW_Loop";
    case TokenType::KW_Goto: return "KW_Goto";
    case TokenType::KW_End: return "KW_End";
    case TokenType::KW_Mod: return "KW_Mod";
    case TokenType::KW_And: return "KW_And";
    case TokenType::KW_Or: return "KW_Or";
    case TokenType::KW_Not: return "KW_Not";
    case TokenType::KW_Sub: return "KW_Sub";
    case TokenType::KW_EndSub: return "KW_EndSub";
    case TokenType::KW_Return: return "KW_Return";
    case TokenType::KW_Break: return "KW_Break";
    case TokenType::KW_Continue: return "KW_Continue";
    case TokenType::Plus: return "Plus";
    case TokenType::Minus: return "Minus";
    case TokenType::Star: return "Star";
    case TokenType::Slash: return "Slash";
    case TokenType::Caret: return "Caret";
    case TokenType::Backslash: return "Backslash";
    case TokenType::Eq: return "Eq";
    case TokenType::Neq: return "Neq";
    case TokenType::Less: return "Less";
    case TokenType::Great: return "Great";
    case TokenType::Lteq: return "Lteq";
    case TokenType::Gteq: return "Gteq";
    case TokenType::Amp: return "Amp";
    case TokenType::LParen: return "LParen";
    case TokenType::RParen: return "RParen";
    case TokenType::Comma: return "Comma";
    case TokenType::Colon: return "Colon";
    case TokenType::EOL: return "EOL";
    case TokenType::EndOfFile: return "EndOfFile";
    case TokenType::KW_ByRef: return "ByRef";
    case TokenType::KW_ByVal: return "ByVal";
    case TokenType::KW_Class: return "Class";
    case TokenType::KW_EndClass: return "EndClass";
    case TokenType::KW_Public: return "Public";
    case TokenType::KW_Private: return "Private";
    case TokenType::KW_Static: return "Static";
    case TokenType::KW_New: return "New";
    case TokenType::Dot: return "Dot";
    default: return "Unknown";
    }
}

std::ostream& operator<<(std::ostream& os, const Token& token) {
  os << "Token{" << to_string(token.type) << ", text=\"" << token.text 
     << "\", line=" << token.start.line << ", col=" << token.start.col << "}";
  return os;
}
