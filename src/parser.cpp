#include "parser.hpp"

Parser::Parser(Lexer &l) : lexer(l) { advance(); }

void Parser::advance() {
  auto next = lexer.next_token();
  if (next)
    current = *next;
  else
    current = Token{TokenType::EndOfFile, ""};
}

bool Parser::consume(TokenType type) {
  if (current.type == type) {
    advance();
    return true;
  }
  return false;
}

int Parser::get_precedence(TokenType type) {
  switch (type) {
  case TokenType::KW_Or:
  case TokenType::KW_And:
    return 2;
  case TokenType::Eq:
  case TokenType::Neq:
  case TokenType::Less:
  case TokenType::Great:
  case TokenType::Lteq:
  case TokenType::Gteq:
    return 5;
  case TokenType::KW_Mod:
  case TokenType::Amp:
    return 10;
  case TokenType::Plus:
  case TokenType::Minus:
    return 20;
  case TokenType::Star:
  case TokenType::Slash:
  case TokenType::Backslash:
    return 30;
  case TokenType::Caret:
    return 40;
  default:
    return 0;
  }
}

std::expected<ExprPtr, std::string> Parser::parse_prefix() {
  if (current.type == TokenType::Number) {
    auto val = current.number;
    advance();
    return std::make_unique<Expr>(Literal{val});
  }
  if (current.type == TokenType::String) {
    auto val = current.text;
    advance();
    return std::make_unique<Expr>(Literal{val});
  }
  if (current.type == TokenType::Char) {
    auto val = current.character;
    advance();
    return std::make_unique<Expr>(Literal{val});
  }
  if (current.type == TokenType::Ident) {
    auto name = current.text;
    advance();
    return std::make_unique<Expr>(Variable{name});
  }
  if (current.type == TokenType::LParen) {
    advance();
    auto expr = parse_expression();
    if (!expr)
      return std::unexpected("Expected expression");
    if (!consume(TokenType::RParen))
      return std::unexpected("Expected ')'");
    return expr;
  }
  if (current.type == TokenType::KW_Not) {
    TokenType op = current.type;
    advance();
    auto operand = parse_expression(2);
    if (!operand)
      return std::unexpected("Expected operand");
    return std::make_unique<Expr>(UnaryOp{op, std::move(*operand)});
  }
  if (current.type == TokenType::Plus || current.type == TokenType::Minus) {
    TokenType op = current.type;
    advance();
    auto operand = parse_expression(35);
    if (!operand)
      return std::unexpected("Expected operand");
    return std::make_unique<Expr>(UnaryOp{op, std::move(*operand)});
  }
  return std::unexpected("Unexpected token in expression");
}

std::expected<ExprPtr, std::string> Parser::parse_infix(ExprPtr left) {
  if (current.type == TokenType::LParen) {
    advance();
    std::vector<ExprPtr> args;
    if (current.type != TokenType::RParen) {
      do {
        auto arg = parse_expression();
        if (!arg)
          return std::unexpected("Invalid argument");
        args.push_back(std::move(*arg));
        if (current.type == TokenType::Comma)
          advance();
        else
          break;
      } while (true);
    }
    if (!consume(TokenType::RParen))
      return std::unexpected("Expected ')' after arguments");
    return std::make_unique<Expr>(Call{std::move(left), std::move(args)});
  }

  TokenType op = current.type;
  int prec = get_precedence(op);
  advance();

  // Right associativity for Caret (^)
  int next_prec = (op == TokenType::Caret) ? prec : prec + 1;
  auto right = parse_expression(next_prec);
  if (!right)
    return right;

  return std::make_unique<Expr>(
      BinaryOp{op, std::move(left), std::move(*right)});
}

std::expected<ExprPtr, std::string> Parser::parse_expression(int precedence) {
  auto left = parse_prefix();
  if (!left)
    return left;

  while (get_precedence(current.type) > precedence) {
    left = parse_infix(std::move(*left));
    if (!left)
      return left;
  }
  return left;
}

std::expected<StmtPtr, std::string> Parser::parse_statement() {
  std::string label;
  if (current.type == TokenType::Ident) {
    // Peek for colon
    // Since we can't peek easily, we check if it's a label or a statement
    // In this grammar, labels are (ident ':').
    // We'll assume if it's an ident and the next is ':', it's a label.
    // This requires a peek. Let's simulate it by checking the next token.
    // For simplicity in this implementation, we'll check if it's a keyword
    // first.
  }

  // Check for label
  if (current.type == TokenType::Ident) {
    // This is ambiguous with expression statements.
    // We need to check if the next token is ':'.
    // Since we don't have a peek(), we'll handle it in the logic.
  }

  // To handle labels correctly, we'd need a peek().
  // Let's assume for now we check keywords first.
  if (current.type == TokenType::KW_Dim)
    return parse_dim();
  if (current.type == TokenType::KW_Let)
    return parse_let();
  if (current.type == TokenType::KW_If)
    return parse_if();
  if (current.type == TokenType::KW_For)
    return parse_for();
  if (current.type == TokenType::KW_Goto)
    return parse_goto();
  if (current.type == TokenType::KW_Do)
    return parse_do();

  // Expression statement
  auto expr = parse_expression();
  if (!expr)
    return std::unexpected("Expected expression");
  if (!consume(TokenType::EOL))
    return std::unexpected("Expected EOL");
  return std::make_unique<Stmt>(ExprStmt{std::move(*expr)});
}

std::expected<StmtPtr, std::string> Parser::parse_dim() {
  advance(); // dim
  std::string name = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected("Expected identifier");
  if (!consume(TokenType::KW_As))
    return std::unexpected("Expected 'as'");
  std::string type = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected("Expected type");
  ExprPtr init = nullptr;
  if (current.type == TokenType::Eq) {
    advance();
    auto e = parse_expression();
    if (!e)
      return std::unexpected("Invalid init expression");
    init = std::move(*e);
  }
  if (!consume(TokenType::EOL))
    return std::unexpected("Expected EOL");
  return std::make_unique<Stmt>(DimStmt{name, type, std::move(init)});
}

std::expected<StmtPtr, std::string> Parser::parse_let() {
  advance(); // let
  std::string name = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected("Expected identifier");
  if (!consume(TokenType::Eq))
    return std::unexpected("Expected '='");
  auto val = parse_expression();
  if (!val)
    return std::unexpected("Expected expression");
  if (!consume(TokenType::EOL))
    return std::unexpected("Expected EOL");
  return std::make_unique<Stmt>(LetStmt{name, std::move(*val)});
}

std::expected<StmtPtr, std::string> Parser::parse_if() {
  advance(); // if
  auto cond = parse_expression();
  if (!cond)
    return std::unexpected("Expected condition");
  if (!consume(TokenType::KW_Then))
    return std::unexpected("Expected 'then'");
  if (!consume(TokenType::EOL))
    return std::unexpected("Expected EOL");

  auto then_branch = parse_statements(TokenType::KW_Else);
  if (!then_branch)
    return std::unexpected("Invalid then branch");

  std::vector<StmtPtr> else_branch;
  while (current.type == TokenType::KW_Else) {
    advance();
    if (current.type == TokenType::KW_If) {
      auto nested_if = parse_if();
      if (!nested_if)
        return std::unexpected("Invalid else if");
      else_branch.push_back(std::move(*nested_if));
    } else {
      auto stmts = parse_statements(TokenType::KW_EndIf);
      if (!stmts)
        return std::unexpected("Invalid else branch");
      for (auto &s : *stmts)
        else_branch.push_back(std::move(s));
    }
  }
  if (!consume(TokenType::KW_EndIf))
    return std::unexpected("Expected 'end if'");
  return std::make_unique<Stmt>(IfStmt{
      std::move(*cond), std::move(*then_branch), std::move(else_branch)});
}

std::expected<StmtPtr, std::string> Parser::parse_for() {
  advance(); // for
  std::string var = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected("Expected identifier");
  if (!consume(TokenType::Eq))
    return std::unexpected("Expected '='");
  auto start = parse_expression();
  if (!start)
    return std::unexpected("Expected start value");
  if (!consume(TokenType::KW_To))
    return std::unexpected("Expected 'to'");
  auto end = parse_expression();
  if (!end)
    return std::unexpected("Expected end value");
  ExprPtr step = nullptr;
  if (current.type == TokenType::KW_Step) {
    advance();
    if (auto step_parse = parse_expression()) {
      step = std::move(*step_parse);
    } else {
      return std::unexpected("Invalid step");
    }
  }
  if (!consume(TokenType::EOL))
    return std::unexpected("Expected EOL");
  auto body = parse_statements(TokenType::KW_EndFor);
  if (!body)
    return std::unexpected("Invalid for body");
  if (!consume(TokenType::KW_EndFor))
    return std::unexpected("Expected 'end for'");
  return std::make_unique<Stmt>(ForStmt{var, std::move(*start), std::move(*end),
                                        std::move(step), std::move(*body)});
}

std::expected<StmtPtr, std::string> Parser::parse_goto() {
  advance(); // goto
  std::string label = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected("Expected label");
  if (!consume(TokenType::EOL))
    return std::unexpected("Expected EOL");
  return std::make_unique<Stmt>(GotoStmt{label});
}

std::expected<StmtPtr, std::string> Parser::parse_do() {
  advance(); // do
  bool while_at_start = (current.type == TokenType::KW_While);
  std::vector<StmtPtr> body;
  ExprPtr cond = nullptr;

  if (while_at_start) {
    advance();
    if (auto cond_parse = parse_expression(); cond_parse) {
      cond = std::move(*cond_parse);
    } else
      return std::unexpected("Invalid while condition");
    if (!consume(TokenType::EOL))
      return std::unexpected("Expected EOL");
    body = *parse_statements(TokenType::KW_Loop);
    if (!consume(TokenType::KW_Loop))
      return std::unexpected("Expected 'loop'");
  } else {
    body = *parse_statements(TokenType::KW_Loop);
    if (!consume(TokenType::KW_Loop))
      return std::unexpected("Expected 'loop'");
    if (!consume(TokenType::KW_While))
      return std::unexpected("Expected 'while'");
    if (auto cond_parse = parse_expression(); cond_parse) {
      cond = std::move(*cond_parse);
    } else
      return std::unexpected("Invalid while condition");
  }
  if (!consume(TokenType::EOL))
    return std::unexpected("Expected EOL");
  return std::make_unique<Stmt>(
      DoStmt{std::move(body), std::move(cond), while_at_start});
}

std::expected<std::vector<StmtPtr>, std::string>
Parser::parse_statements(TokenType stop_token) {
  std::vector<StmtPtr> stmts;
  while (current.type != stop_token && current.type != TokenType::EndOfFile) {
    auto stmt = parse_statement();
    if (!stmt)
      return std::unexpected(stmt.error());
    stmts.push_back(std::move(*stmt));
  }
  return stmts;
}

std::expected<Program, std::string> Parser::parse() {
  auto stmts = parse_statements();
  if (!stmts)
    return std::unexpected(stmts.error());
  return Program{std::move(*stmts)};
}
