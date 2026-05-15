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
  case TokenType::LParen:
    return 50;
  default:
    return 0;
  }
}

std::expected<ExprPtr, Error> Parser::parse_prefix() {
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
      return std::unexpected(expr.error());
    if (!consume(TokenType::RParen))
      return std::unexpected(Error{"Expected ')'", current.line, current.col});
    return expr;
  }
  if (current.type == TokenType::KW_Not) {
    TokenType op = current.type;
    advance();
    auto operand = parse_expression(2);
    if (!operand)
      return std::unexpected(operand.error());
    return std::make_unique<Expr>(UnaryOp{op, std::move(*operand)});
  }
  if (current.type == TokenType::Plus || current.type == TokenType::Minus) {
    TokenType op = current.type;
    advance();
    auto operand = parse_expression(35);
    if (!operand)
      return std::unexpected(operand.error());
    return std::make_unique<Expr>(UnaryOp{op, std::move(*operand)});
  }
  return std::unexpected(Error{"Unexpected token in expression", current.line, current.col});
}

std::expected<ExprPtr, Error> Parser::parse_infix(ExprPtr left) {
  if (current.type == TokenType::LParen) {
    advance();
    std::vector<ExprPtr> args;
    if (current.type != TokenType::RParen) {
      do {
        auto arg = parse_expression();
        if (!arg)
          return std::unexpected(arg.error());
        args.push_back(std::move(*arg));
        if (current.type == TokenType::Comma)
          advance();
        else
          break;
      } while (true);
    }
    if (!consume(TokenType::RParen))
      return std::unexpected(Error{"Expected ')' after arguments", current.line, current.col});
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

std::expected<ExprPtr, Error> Parser::parse_expression(int precedence) {
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

std::expected<StmtPtr, Error> Parser::parse_statement() {
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
    return std::unexpected(expr.error());
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{"Expected EOL", current.line, current.col});
  while (consume(TokenType::EOL)) ;
  return std::make_unique<Stmt>(ExprStmt{std::move(*expr)});
}

std::expected<StmtPtr, Error> Parser::parse_dim() {
  advance(); // dim
  std::string name = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected(Error{"Expected identifier", current.line, current.col});
  if (!consume(TokenType::KW_As))
    return std::unexpected(Error{"Expected 'as'", current.line, current.col});
  std::string type = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected(Error{"Expected type", current.line, current.col});
  ExprPtr init = nullptr;
  if (current.type == TokenType::Eq) {
    advance();
    auto e = parse_expression();
    if (!e)
      return std::unexpected(e.error());
    init = std::move(*e);
  }
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{"Expected EOL", current.line, current.col});
  while (consume(TokenType::EOL)) ;
  return std::make_unique<Stmt>(DimStmt{name, type, std::move(init)});
}

std::expected<StmtPtr, Error> Parser::parse_let() {
  advance(); // let
  std::string name = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected(Error{"Expected identifier", current.line, current.col});
  if (!consume(TokenType::Eq))
    return std::unexpected(Error{"Expected '='", current.line, current.col});
  auto val = parse_expression();
  if (!val)
    return std::unexpected(val.error());
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{"Expected EOL", current.line, current.col});
  while (consume(TokenType::EOL)) ;
  return std::make_unique<Stmt>(LetStmt{name, std::move(*val)});
}

std::expected<StmtPtr, Error> Parser::parse_if() {
  advance(); // if
  auto cond = parse_expression();
  if (!cond)
    return std::unexpected(cond.error());
  if (!consume(TokenType::KW_Then))
    return std::unexpected(Error{"Expected 'then'", current.line, current.col});
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{"Expected EOL", current.line, current.col});
  while (consume(TokenType::EOL)) ;

  auto then_branch = parse_statements({TokenType::KW_Else, TokenType::KW_EndIf});
  if (!then_branch)
    return std::unexpected(then_branch.error());

  auto else_branch = parse_else_branch();
  if (!else_branch)
    return std::unexpected(else_branch.error());

  if (!consume(TokenType::EOL))
    return std::unexpected(Error{"Expected EOL", current.line, current.col});
  while (consume(TokenType::EOL)) ;

  return std::make_unique<Stmt>(IfStmt{
      std::move(*cond), std::move(*then_branch), std::move(*else_branch)});
}

std::expected<ElseBranch, Error> Parser::parse_else_branch() {
  if (current.type == TokenType::KW_EndIf) {
    advance();
    return std::monostate{};
  }

  if (current.type == TokenType::KW_Else) {
    advance(); // else
    if (current.type == TokenType::KW_If) {
      advance(); // if
      auto cond = parse_expression();
      if (!cond)
        return std::unexpected(cond.error());
      if (!consume(TokenType::KW_Then))
        return std::unexpected(Error{"Expected 'then'", current.line, current.col});
      if (!consume(TokenType::EOL))
        return std::unexpected(Error{"Expected EOL", current.line, current.col});
      while (consume(TokenType::EOL)) ;

      auto then_branch = parse_statements({TokenType::KW_Else, TokenType::KW_EndIf});
      if (!then_branch)
        return std::unexpected(then_branch.error());

      auto else_branch = parse_else_branch();
      if (!else_branch)
        return std::unexpected(else_branch.error());

      return std::make_unique<Stmt>(IfStmt{
          std::move(*cond), std::move(*then_branch), std::move(*else_branch)});
    } else {
      auto else_stmts = parse_statements({TokenType::KW_EndIf});
      if (!else_stmts)
        return std::unexpected(else_stmts.error());
      if (!consume(TokenType::KW_EndIf))
        return std::unexpected(Error{"Expected 'end if'", current.line, current.col});
      return std::move(*else_stmts);
    }
  }

  return std::unexpected(Error{"Expected 'else' or 'end if'", current.line, current.col});
}

std::expected<StmtPtr, Error> Parser::parse_for() {
  advance(); // for
  std::string var = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected(Error{"Expected identifier", current.line, current.col});
  if (!consume(TokenType::Eq))
    return std::unexpected(Error{"Expected '='", current.line, current.col});
  auto start = parse_expression();
  if (!start)
    return std::unexpected(start.error());
  if (!consume(TokenType::KW_To))
    return std::unexpected(Error{"Expected 'to'", current.line, current.col});
  auto end = parse_expression();
  if (!end)
    return std::unexpected(end.error());
  ExprPtr step = nullptr;
  if (current.type == TokenType::KW_Step) {
    advance();
    if (auto step_parse = parse_expression()) {
      step = std::move(*step_parse);
    } else {
      return std::unexpected(step_parse.error());
    }
  }
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{"Expected EOL", current.line, current.col});
  while (consume(TokenType::EOL)) ;
  auto body = parse_statements({TokenType::KW_EndFor});
  if (!body)
    return std::unexpected(body.error());
  if (!consume(TokenType::KW_EndFor))
    return std::unexpected(Error{"Expected 'end for'", current.line, current.col});
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{"Expected EOL", current.line, current.col});
  while (consume(TokenType::EOL)) ;
  return std::make_unique<Stmt>(ForStmt{var, std::move(*start), std::move(*end),
                                        std::move(step), std::move(*body)});
}

std::expected<StmtPtr, Error> Parser::parse_goto() {
  advance(); // goto
  std::string label = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected(Error{"Expected label", current.line, current.col});
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{"Expected EOL", current.line, current.col});
  return std::make_unique<Stmt>(GotoStmt{label});
}

std::expected<StmtPtr, Error> Parser::parse_do() {
  advance(); // do
  bool while_at_start = (current.type == TokenType::KW_While);
  std::vector<StmtPtr> body;
  ExprPtr cond = nullptr;

  if (while_at_start) {
    advance();
    if (auto cond_parse = parse_expression(); cond_parse) {
      cond = std::move(*cond_parse);
    } else
      return std::unexpected(cond_parse.error());
    if (!consume(TokenType::EOL))
      return std::unexpected(Error{"Expected EOL", current.line, current.col});
    while (consume(TokenType::EOL)) ;
    auto body_parse = parse_statements({TokenType::KW_Loop});
    if (!body_parse)
      return std::unexpected(body_parse.error());
    body = std::move(*body_parse);
    if (!consume(TokenType::KW_Loop))
      return std::unexpected(Error{"Expected 'loop'", current.line, current.col});
  } else {
    auto body_parse = parse_statements({TokenType::KW_Loop});
    if (!body_parse)
      return std::unexpected(body_parse.error());
    body = std::move(*body_parse);
    if (!consume(TokenType::KW_Loop))
      return std::unexpected(Error{"Expected 'loop'", current.line, current.col});
    if (!consume(TokenType::KW_While))
      return std::unexpected(Error{"Expected 'while'", current.line, current.col});
    if (auto cond_parse = parse_expression(); cond_parse) {
      cond = std::move(*cond_parse);
    } else
      return std::unexpected(cond_parse.error());
  }
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{"Expected EOL", current.line, current.col});
  while (consume(TokenType::EOL)) ;
  return std::make_unique<Stmt>(
      DoStmt{std::move(body), std::move(cond), while_at_start});
}

std::expected<std::vector<StmtPtr>, Error>
Parser::parse_statements(std::vector<TokenType> stop_tokens) {
  std::vector<StmtPtr> stmts;
  while (true) {
    bool stop = false;
    for (auto t : stop_tokens) {
      if (current.type == t) {
        stop = true;
        break;
      }
    }
    if (stop || current.type == TokenType::EndOfFile)
      break;

    auto stmt = parse_statement();
    if (!stmt)
      return std::unexpected(stmt.error());
    stmts.push_back(std::move(*stmt));
  }
  return stmts;
}

std::expected<Program, Error> Parser::parse() {
  auto stmts = parse_statements();
  if (!stmts)
    return std::unexpected(stmts.error());
  return Program{std::move(*stmts)};
}
