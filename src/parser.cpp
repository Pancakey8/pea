#include "parser.hpp"
#include "lexer.hpp"

Parser::Parser(Lexer &l) : lexer(l) { advance(); }

void Parser::advance() {
  last = current;
  if (peeked_token) {
    current = *peeked_token;
    peeked_token = std::nullopt;
  } else {
    auto next = lexer.next_token();
    if (next)
      current = *next;
    else
      current = Token{ TokenType::EndOfFile, "", 0, 0, { 1, 1 }, { 1, 1 } };
  }
}

Token Parser::peek() {
  if (peeked_token)
    return *peeked_token;
  auto next = lexer.next_token();
  if (next) {
    peeked_token = *next;
    return *next;
  }
  return Token{ TokenType::EndOfFile, "", 0, 0, { 1, 1 }, { 1, 1 } };
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
    SourceRange range = { current.start, current.end };
    advance();
    return std::make_unique<Expr>(Literal{ val }, range);
  }
  if (current.type == TokenType::String) {
    auto val = current.text;
    SourceRange range = { current.start, current.end };
    advance();
    return std::make_unique<Expr>(Literal{ val }, range);
  }
  if (current.type == TokenType::Char) {
    auto val = current.character;
    SourceRange range = { current.start, current.end };
    advance();
    return std::make_unique<Expr>(Literal{ val }, range);
  }
  if (current.type == TokenType::Ident) {
    auto name = current.text;
    SourceRange range = { current.start, current.end };
    advance();
    return std::make_unique<Expr>(Variable{ name }, range);
  }
  if (current.type == TokenType::LParen) {
    Token start_t = current;
    advance();
    auto expr = parse_expression();
    if (!expr)
      return std::unexpected(expr.error());
    if (!consume(TokenType::RParen))
      return std::unexpected(
        Error{ "Expected ')'", current.start, current.end });
    return expr;
  }
  if (current.type == TokenType::KW_Not) {
    Token start_t = current;
    TokenType op = current.type;
    advance();
    auto operand = parse_expression(2);
    if (!operand)
      return std::unexpected(operand.error());
    return std::make_unique<Expr>(UnaryOp{ op, std::move(*operand) },
      SourceRange{ start_t.start, (*operand)->range.end });
  }
  if (current.type == TokenType::Plus || current.type == TokenType::Minus) {
    Token start_t = current;
    TokenType op = current.type;
    advance();
    auto operand = parse_expression(35);
    if (!operand)
      return std::unexpected(operand.error());
    return std::make_unique<Expr>(UnaryOp{ op, std::move(*operand) },
      SourceRange{ start_t.start, (*operand)->range.end });
  }
  return std::unexpected(
    Error{ "Unexpected token in expression", current.start, current.end });
}

std::expected<ExprPtr, Error> Parser::parse_infix(ExprPtr left) {
  if (current.type == TokenType::LParen) {
    Token start_t = current;
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
      return std::unexpected(
        Error{ "Expected ')' after arguments", current.start, current.end });
    return std::make_unique<Expr>(Call{ std::move(left), std::move(args) },
      SourceRange{ left->range.start, last.end });
  }

  Token op_t = current;
  TokenType op = current.type;
  int prec = get_precedence(op);
  advance();

  // Right associativity for Caret (^)
  int next_prec = (op == TokenType::Caret) ? prec - 1 : prec + 1;
  auto right = parse_expression(next_prec);
  if (!right)
    return right;

  return std::make_unique<Expr>(
    BinaryOp{ op, std::move(left), std::move(*right) },
    SourceRange{ left->range.start, (*right)->range.end });
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
  if (current.type == TokenType::Ident && peek().type == TokenType::Colon) {
    Token start_t = current;
    std::string name = current.text;
    advance(); // ident
    advance(); // colon
    return std::make_unique<Stmt>(
      LabelStmt{ name }, SourceRange{ start_t.start, last.end });
  }
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
  if (current.type == TokenType::KW_Sub)
    return parse_sub();
  if (current.type == TokenType::KW_Return) {
    if (!in_subroutine)
      return std::unexpected(
        Error{ "Return statement not allowed outside of subroutine",
          current.start,
          current.end });
    return parse_return();
  }
  if (current.type == TokenType::KW_Break) {
    if (!in_loop)
      return std::unexpected(
        Error{ "Break statement not allowed outside of loop",
          current.start,
          current.end });
    return parse_break();
  }
  if (current.type == TokenType::KW_Continue) {
    if (!in_loop)
      return std::unexpected(
        Error{ "Continue statement not allowed outside of loop",
          current.start,
          current.end });
    return parse_continue();
  }

  // Expression statement
  Token start_t = current;
  auto expr = parse_expression();
  if (!expr)
    return std::unexpected(expr.error());

  Token last_t = current;
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL) {
    last_t = current;
    advance();
  }
  return std::make_unique<Stmt>(
    ExprStmt{ std::move(*expr) }, SourceRange{ start_t.start, last_t.end });
}

std::expected<StmtPtr, Error> Parser::parse_dim() {
  Token start_t = current;
  advance(); // dim
  std::string name = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected(
      Error{ "Expected identifier", current.start, current.end });

  std::vector<ExprPtr> dims{};
  if (consume(TokenType::LParen)) {
    if (current.type != TokenType::RParen) {
      do {
        auto arg = parse_expression();
        if (!arg)
          return std::unexpected(arg.error());
        dims.push_back(std::move(*arg));
        if (current.type == TokenType::Comma)
          advance();
        else
          break;
      } while (true);
    }

    if (!consume(TokenType::RParen))
      return std::unexpected(
        Error{ "Expected ')' after dimensions", current.start, current.end });
  }

  std::optional<std::string> type{};
  if (consume(TokenType::KW_As)) {
    if (current.type != TokenType::Ident)
      return std::unexpected(
        Error{ "Expected type after 'as'", current.start, current.end });
    type = current.text;
    advance();
  }

  std::optional<ExprPtr> init{};
  if (consume(TokenType::Eq)) {
    auto val = parse_expression();
    if (!val)
      return std::unexpected(val.error());
    init = std::move(*val);
  }

  Token last_t = current;
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL) {
    last_t = current;
    advance();
  }
  return std::make_unique<Stmt>(
    DimStmt{ name, std::move(dims), std::move(type), std::move(init) },
    SourceRange{ start_t.start, last_t.end });
}

std::expected<StmtPtr, Error> Parser::parse_let() {
  Token start_t = current;
  advance(); // let
  std::string name = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected(
      Error{ "Expected identifier", current.start, current.end });
  if (!consume(TokenType::Eq))
    return std::unexpected(Error{ "Expected '='", current.start, current.end });
  auto val = parse_expression();
  if (!val)
    return std::unexpected(val.error());
  Token last_t = current;
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL) {
    last_t = current;
    advance();
  }
  return std::make_unique<Stmt>(
    LetStmt{ name, std::move(*val) }, SourceRange{ start_t.start, last_t.end });
}

std::expected<StmtPtr, Error> Parser::parse_if() {
  Token start_t = current;
  advance(); // if
  auto cond = parse_expression();
  if (!cond)
    return std::unexpected(cond.error());
  if (!consume(TokenType::KW_Then))
    return std::unexpected(
      Error{ "Expected 'then'", current.start, current.end });
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL)
    advance();

  auto then_branch =
    parse_statements({ TokenType::KW_Else, TokenType::KW_EndIf });
  if (!then_branch)
    return std::unexpected(then_branch.error());

  auto else_branch = parse_else_branch();
  if (!else_branch)
    return std::unexpected(else_branch.error());

  Token last_t = last;
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL) {
    last_t = current;
    advance();
  }

  return std::make_unique<Stmt>(
    IfStmt{
      std::move(*cond), std::move(*then_branch), std::move(*else_branch) },
    SourceRange{ start_t.start, last_t.end });
}

std::expected<ElseBranch, Error> Parser::parse_else_branch() {
  Token start_t = current;
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
        return std::unexpected(
          Error{ "Expected 'then'", current.start, current.end });
      if (!consume(TokenType::EOL))
        return std::unexpected(
          Error{ "Expected EOL", current.start, current.end });
      while (current.type == TokenType::EOL)
        advance();

      auto then_branch =
        parse_statements({ TokenType::KW_Else, TokenType::KW_EndIf });
      if (!then_branch)
        return std::unexpected(then_branch.error());

      auto else_branch = parse_else_branch();
      if (!else_branch)
        return std::unexpected(else_branch.error());

      Token last_t = current;
      return std::make_unique<Stmt>(
        IfStmt{
          std::move(*cond), std::move(*then_branch), std::move(*else_branch) },
        SourceRange{ start_t.start, last_t.end });
    } else {
      if (!consume(TokenType::EOL))
        return std::unexpected(
          Error{ "Expected EOL", current.start, current.end });
      while (current.type == TokenType::EOL)
        advance();
      auto else_stmts = parse_statements({ TokenType::KW_EndIf });
      if (!else_stmts)
        return std::unexpected(else_stmts.error());
      if (!consume(TokenType::KW_EndIf))
        return std::unexpected(
          Error{ "Expected 'end if'", current.start, current.end });
      return std::move(*else_stmts);
    }
  }

  return std::unexpected(
    Error{ "Expected 'else' or 'end if'", current.start, current.end });
}

std::expected<StmtPtr, Error> Parser::parse_for() {
  Token start_t = current;
  advance(); // for
  std::string var = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected(
      Error{ "Expected identifier", current.start, current.end });
  if (!consume(TokenType::Eq))
    return std::unexpected(Error{ "Expected '='", current.start, current.end });
  auto start = parse_expression();
  if (!start)
    return std::unexpected(start.error());
  if (!consume(TokenType::KW_To))
    return std::unexpected(
      Error{ "Expected 'to'", current.start, current.end });
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
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL)
    advance();

  bool old_loop = in_loop;
  in_loop = true;
  auto body = parse_statements({ TokenType::KW_EndFor });
  in_loop = old_loop;

  if (!body)
    return std::unexpected(body.error());
  if (!consume(TokenType::KW_EndFor))
    return std::unexpected(
      Error{ "Expected 'end for'", current.start, current.end });
  Token last_t = last;
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL) {
    last_t = current;
    advance();
  }
  return std::make_unique<Stmt>(ForStmt{ var,
                                  std::move(*start),
                                  std::move(*end),
                                  std::move(step),
                                  std::move(*body) },
    SourceRange{ start_t.start, last_t.end });
}

std::expected<StmtPtr, Error> Parser::parse_goto() {
  Token start_t = current;
  advance(); // goto
  std::string label = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected(
      Error{ "Expected label", current.start, current.end });
  Token last_t = current;
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL) {
    last_t = current;
    advance();
  }
  return std::make_unique<Stmt>(
    GotoStmt{ label }, SourceRange{ start_t.start, last_t.end });
}

std::expected<StmtPtr, Error> Parser::parse_do() {
  Token start_t = current;
  advance(); // do
  bool while_at_start = (current.type == TokenType::KW_While);
  std::vector<StmtPtr> body;
  ExprPtr cond = nullptr;

  bool old_loop = in_loop;
  in_loop = true;

  if (while_at_start) {
    advance();
    if (auto cond_parse = parse_expression(); cond_parse) {
      cond = std::move(*cond_parse);
    } else
      return std::unexpected(cond_parse.error());
    if (!consume(TokenType::EOL))
      return std::unexpected(
        Error{ "Expected EOL", current.start, current.end });
    while (current.type == TokenType::EOL)
      advance();
    auto body_parse = parse_statements({ TokenType::KW_Loop });
    if (!body_parse)
      return std::unexpected(body_parse.error());
    body = std::move(*body_parse);
    if (!consume(TokenType::KW_Loop))
      return std::unexpected(
        Error{ "Expected 'loop'", current.start, current.end });
  } else {
    auto body_parse = parse_statements({ TokenType::KW_Loop });
    if (!body_parse)
      return std::unexpected(body_parse.error());
    body = std::move(*body_parse);
    if (!consume(TokenType::KW_Loop))
      return std::unexpected(
        Error{ "Expected 'loop'", current.start, current.end });
    if (!consume(TokenType::KW_While))
      return std::unexpected(
        Error{ "Expected 'while'", current.start, current.end });
    if (auto cond_parse = parse_expression(); cond_parse) {
      cond = std::move(*cond_parse);
    } else
      return std::unexpected(cond_parse.error());
  }
  in_loop = old_loop;

  Token last_t = last;
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL) {
    last_t = current;
    advance();
  }
  return std::make_unique<Stmt>(
    DoStmt{ std::move(body), std::move(cond), while_at_start },
    SourceRange{ start_t.start, last_t.end });
}

std::expected<StmtPtr, Error> Parser::parse_sub() {
  Token start_t = current;
  advance(); // sub
  std::string name = current.text;
  if (!consume(TokenType::Ident))
    return std::unexpected(
      Error{ "Expected subroutine name", current.start, current.end });
  if (!consume(TokenType::LParen))
    return std::unexpected(Error{ "Expected '('", current.start, current.end });

  std::vector<Parameter> params;
  while (current.type == TokenType::Ident) {
    std::string p_name = current.text;
    advance();
    std::optional<std::string> p_type{};
    if (consume(TokenType::KW_As)) {
      if (current.type != TokenType::Ident)
	return std::unexpected(
			       Error{ "Expected parameter type", current.start, current.end });
      p_type = current.text;
      advance();
    }
    params.push_back({ p_name, p_type });
    if (!consume(TokenType::Comma))
      break;
  }

  if (!consume(TokenType::RParen))
    return std::unexpected(Error{ "Expected ')'", current.start, current.end });

  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL)
    advance();

  bool old_sub = in_subroutine;
  in_subroutine = true;
  auto body = parse_statements({ TokenType::KW_EndSub });
  in_subroutine = old_sub;

  if (!body)
    return std::unexpected(body.error());

  if (!consume(TokenType::KW_EndSub))
    return std::unexpected(
      Error{ "Expected 'end sub'", current.start, current.end });

  Token last_t = current;
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL) {
    last_t = current;
    advance();
  }

  return std::make_unique<Stmt>(
    SubDecl{ name, std::move(params), std::move(*body) },
    SourceRange{ start_t.start, last_t.end });
}

std::expected<StmtPtr, Error> Parser::parse_return() {
  Token start_t = current;
  advance(); // return
  ExprPtr val = nullptr;
  if (current.type != TokenType::EOL && current.type != TokenType::EndOfFile) {
    auto e = parse_expression();
    if (!e)
      return std::unexpected(e.error());
    val = std::move(*e);
  }
  Token last_t = current;
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL) {
    last_t = current;
    advance();
  }
  return std::make_unique<Stmt>(
    ReturnStmt{ std::move(val) }, SourceRange{ start_t.start, last_t.end });
}

std::expected<StmtPtr, Error> Parser::parse_break() {
  Token start_t = current;
  advance(); // break
  Token last_t = current;
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL) {
    last_t = current;
    advance();
  }
  return std::make_unique<Stmt>(
    BreakStmt{}, SourceRange{ start_t.start, last_t.end });
}

std::expected<StmtPtr, Error> Parser::parse_continue() {
  Token start_t = current;
  advance(); // continue
  Token last_t = current;
  if (!consume(TokenType::EOL))
    return std::unexpected(Error{ "Expected EOL", current.start, current.end });
  while (current.type == TokenType::EOL) {
    last_t = current;
    advance();
  }
  return std::make_unique<Stmt>(
    ContinueStmt{}, SourceRange{ start_t.start, last_t.end });
}

std::expected<std::vector<StmtPtr>, Error> Parser::parse_statements(
  std::vector<TokenType> stop_tokens) {
  while (consume(TokenType::EOL))
    ;
  std::vector<StmtPtr> stmts;
  while (true) {
    while (consume(TokenType::EOL))
      ;
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

  SourceRange range = { { 1, 1 }, { 1, 1 } };
  if (!stmts->empty()) {
    range = { (*stmts)[0]->range.start, (*stmts).back()->range.end };
  }
  return Program{ std::move(*stmts), range };
}
