#pragma once
#include "lexer.hpp"
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <ostream>

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Literal {
  std::variant<double, std::string, char> value;
};
struct Variable {
  std::string name;
};
struct BinaryOp {
  TokenType op;
  ExprPtr left;
  ExprPtr right;
};
struct UnaryOp {
  TokenType op;
  ExprPtr operand;
};
struct Call {
  ExprPtr callee;
  std::vector<ExprPtr> args;
};

struct Expr {
  std::variant<Literal, Variable, BinaryOp, UnaryOp, Call> data;
  SourceRange range;
  Expr(auto &&val, SourceRange range) : data(std::forward<decltype(val)>(val)), range(range) {}
};

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;

struct DimStmt {
  std::string name;
  std::vector<ExprPtr> dims;
  std::optional<std::string> type;
  std::optional<ExprPtr> init;
};
struct LetStmt {
  std::string name;
  ExprPtr value;
};

using ElseBranch = std::variant<std::monostate, std::vector<StmtPtr>, StmtPtr>;

struct IfStmt {
  ExprPtr condition;
  std::vector<StmtPtr> then_branch;
  ElseBranch else_branch;
};
struct ForStmt {
  std::string var;
  ExprPtr start;
  ExprPtr end;
  ExprPtr step;
  std::vector<StmtPtr> body;
};
struct GotoStmt {
  std::string label;
};
struct DoStmt {
  std::vector<StmtPtr> body;
  ExprPtr condition;
  bool while_at_start;
};
struct ExprStmt {
  ExprPtr expr;
};
struct LabelStmt {
  std::string name;
};

struct Parameter {
  std::string name;
  std::string type;
};
struct SubDecl {
  std::string name;
  std::vector<Parameter> params;
  std::vector<StmtPtr> body;
};

struct ReturnStmt {
  ExprPtr value;
};
struct BreakStmt {};
struct ContinueStmt {};

struct Stmt {
  std::variant<DimStmt, LetStmt, IfStmt, ForStmt, GotoStmt, DoStmt, ExprStmt,
               LabelStmt, SubDecl, ReturnStmt, BreakStmt, ContinueStmt>
      data;
  SourceRange range;
  Stmt(auto &&val, SourceRange range) : data(std::forward<decltype(val)>(val)), range(range) {}
};

struct Program {
  std::vector<StmtPtr> statements;
  SourceRange range;
};

std::ostream& operator<<(std::ostream& os, const Program& prog);
std::ostream& operator<<(std::ostream& os, const Stmt& stmt);
std::ostream& operator<<(std::ostream& os, const Expr& expr);
