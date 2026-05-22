#include "ast.hpp"
#include "lexer.hpp"
#include <ostream>
#include <string>
#include <variant>

// Helper to escape JSON strings
static std::string escape_json_string(const std::string &s) {
  std::string result;
  for (char c : s) {
    if (c == '"')
      result += "\\\"";
    else if (c == '\\')
      result += "\\\\";
    else if (c == '\n')
      result += "\\n";
    else if (c == '\r')
      result += "\\r";
    else if (c == '\t')
      result += "\\t";
    else
      result += c;
  }
  return result;
}

// Helper to print JSON strings
static std::ostream &op_json_str(std::ostream &os, const std::string &s) {
  return os << "\"" << s << "\"";
}

// Forward declarations for recursive printing
std::ostream &operator<<(std::ostream &os, const Expr &expr);
std::ostream &operator<<(std::ostream &os, const Stmt &stmt);

struct ExprPrinter {
  std::ostream &os;

  void operator()(const Literal &l) {
    os << "{\"type\":\"Literal\",\"value\":";
    std::visit(
      [this](auto &&val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, double>) {
          os << val;
        } else if constexpr (std::is_same_v<T, std::string>) {
          op_json_str(os, escape_json_string(val));
        } else if constexpr (std::is_same_v<T, char>) {
          os << "\"";
          os << val;
          os << "\"";
        }
      },
      l.value);
    os << "}";
  }

  void operator()(const Variable &v) {
    os << "{\"type\":\"Variable\",\"name\":";
    op_json_str(os, v.name);
    os << "}";
  }

  void operator()(const BinaryOp &b) {
    os << "{\"type\":\"BinaryOp\",\"op\":";
    op_json_str(os, to_string(b.op));
    os << ",\"left\":";
    if (b.left)
      os << *b.left;
    else
      os << "null";
    os << ",\"right\":";
    if (b.right)
      os << *b.right;
    else
      os << "null";
    os << "}";
  }

  void operator()(const UnaryOp &u) {
    os << "{\"type\":\"UnaryOp\",\"op\":";
    op_json_str(os, to_string(u.op));
    os << ",\"operand\":";
    if (u.operand)
      os << *u.operand;
    else
      os << "null";
    os << "}";
  }

  void operator()(const Call &c) {
    os << "{\"type\":\"Call\",\"callee\":";
    if (c.callee)
      os << *c.callee;
    else
      os << "null";
    os << ",\"args\":[";
    for (size_t i = 0; i < c.args.size(); ++i) {
      if (c.args[i])
        os << *c.args[i];
      else
        os << "null";
      if (i < c.args.size() - 1)
        os << ",";
    }
    os << "]}";
  }
};

struct StmtPrinter {
  std::ostream &os;

  void operator()(const DimStmt &s) {
    os << "{\"type\":\"DimStmt\",\"name\":";
    op_json_str(os, s.name);
    os << ",\"dims\":[";
    for (size_t i = 0; i < s.dims.size(); ++i) {
      if (s.dims[i])
        os << *s.dims[i];
      else
        os << "null";
      if (i < s.dims.size() - 1)
        os << ",";
    }
    os << "],\"type\":";
    if (s.type)
      os << "\"" << *s.type << "\"";
    else
      os << "null";
    os << ",\"init\":";
    if (s.init)
      os << *s.init.value();
    else
      os << "null";
    os << "}";
  }

  void operator()(const LetStmt &s) {
    os << "{\"type\":\"LetStmt\",\"name\":";
    op_json_str(os, s.name);
    if (s.dims.size() > 0) {
      os << ",\"dims\":[";
      for (size_t i = 0; i < s.dims.size(); ++i) {
        if (s.dims[i])
          os << *s.dims[i];
        else
          os << "null";
        if (i < s.dims.size() - 1)
          os << ",";
      }
      os << ']';
    }
    os << ",\"value\":";
    if (s.value)
      os << *s.value;
    else
      os << "null";
    os << "}";
  }

  void operator()(const IfStmt &s) {
    os << "{\"type\":\"IfStmt\",\"condition\":";
    if (s.condition)
      os << *s.condition;
    else
      os << "null";
    os << ",\"then_branch\":[";
    for (size_t i = 0; i < s.then_branch.size(); ++i) {
      if (s.then_branch[i])
        os << *s.then_branch[i];
      else
        os << "null";
      if (i < s.then_branch.size() - 1)
        os << ",";
    }
    os << "],\"else_branch\":";
    std::visit(
      [this](auto &&branch) {
        using T = std::decay_t<decltype(branch)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          os << "null";
        } else if constexpr (std::is_same_v<T, std::vector<StmtPtr>>) {
          os << "[";
          for (size_t i = 0; i < branch.size(); ++i) {
            if (branch[i])
              os << *branch[i];
            else
              os << "null";
            if (i < branch.size() - 1)
              os << ",";
          }
          os << "]";
        } else if constexpr (std::is_same_v<T, StmtPtr>) {
          if (branch)
            os << *branch;
          else
            os << "null";
        }
      },
      s.else_branch);
    os << "}";
  }

  void operator()(const ForStmt &s) {
    os << "{\"type\":\"ForStmt\",\"var\":";
    op_json_str(os, s.var);
    os << ",\"start\":";
    if (s.start)
      os << *s.start;
    else
      os << "null";
    os << ",\"end\":";
    if (s.end)
      os << *s.end;
    else
      os << "null";
    os << ",\"step\":";
    if (s.step)
      os << *s.step;
    else
      os << "null";
    os << ",\"body\":[";
    for (size_t i = 0; i < s.body.size(); ++i) {
      if (s.body[i])
        os << *s.body[i];
      else
        os << "null";
      if (i < s.body.size() - 1)
        os << ",";
    }
    os << "]}";
  }

  void operator()(const GotoStmt &s) {
    os << "{\"type\":\"GotoStmt\",\"label\":";
    op_json_str(os, s.label);
    os << "}";
  }

  void operator()(const DoStmt &s) {
    os << "{\"type\":\"DoStmt\",\"body\":[";
    for (size_t i = 0; i < s.body.size(); ++i) {
      if (s.body[i])
        os << *s.body[i];
      else
        os << "null";
      if (i < s.body.size() - 1)
        os << ",";
    }
    os << "],\"condition\":";
    if (s.condition)
      os << *s.condition;
    else
      os << "null";
    os << ",\"while_at_start\":";
    os << (s.while_at_start ? "true" : "false");
    os << "}";
  }

  void operator()(const ExprStmt &s) {
    os << "{\"type\":\"ExprStmt\",\"expr\":";
    if (s.expr)
      os << *s.expr;
    else
      os << "null";
    os << "}";
  }

  void operator()(const LabelStmt &s) {
    os << "{\"type\":\"LabelStmt\",\"name\":";
    op_json_str(os, s.name);
    os << "}";
  }

  void operator()(const SubDecl &s) {
    os << "{\"type\":\"SubDecl\",\"name\":";
    op_json_str(os, s.name);
    os << ",\"params\":[";
    for (size_t i = 0; i < s.params.size(); ++i) {
      os << "{\"name\":";
      op_json_str(os, s.params[i].name);
      os << ",\"type\":";
      if (s.params[i].type)
        op_json_str(os, *s.params[i].type);
      else
        os << "null";
      os << "}";
      if (i < s.params.size() - 1)
        os << ",";
    }
    os << "],\"body\":[";
    for (size_t i = 0; i < s.body.size(); ++i) {
      if (s.body[i])
        os << *s.body[i];
      else
        os << "null";
      if (i < s.body.size() - 1)
        os << ",";
    }
    os << "]}";
  }

  void operator()(const ReturnStmt &s) {
    os << "{\"type\":\"ReturnStmt\",\"value\":";
    if (s.value)
      os << *s.value;
    else
      os << "null";
    os << "}";
  }

  void operator()(const BreakStmt &s) { os << "{\"type\":\"BreakStmt\"}"; }

  void operator()(const ContinueStmt &s) {
    os << "{\"type\":\"ContinueStmt\"}";
  }
};

std::ostream &operator<<(std::ostream &os, const Expr &expr) {
  std::visit(ExprPrinter{ os }, expr.data);
  return os;
}

std::ostream &operator<<(std::ostream &os, const Stmt &stmt) {
  std::visit(StmtPrinter{ os }, stmt.data);
  return os;
}

std::ostream &operator<<(std::ostream &os, const Program &prog) {
  os << "{\"statements\":[";
  for (size_t i = 0; i < prog.statements.size(); ++i) {
    if (prog.statements[i])
      os << *prog.statements[i];
    else
      os << "null";
    if (i < prog.statements.size() - 1)
      os << ",";
  }
  os << "]}";
  return os;
}
