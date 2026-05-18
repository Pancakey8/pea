#include "irgen.hpp"
#include "ast.hpp"
#include "lexer.hpp"
#include <iostream>
#include <print>
#include <utility>

template <class... Fs> struct Overloaded : Fs... {
  using Fs::operator()...;
};

std::expected<ProgramIr, Error> IrGen::generate(Program const &prog) {
  if (auto res = emit(prog); !res)
    return std::unexpected(res.error());

  return ProgramIr{
    std::move(this->prog),
    std::move(variables),
    std::move(consts),
    std::move(labels),
    std::move(types),
  };
}

std::expected<void, Error> IrGen::emit(Program const &prog) {
  for (auto const &stmt : prog.statements) {
    if (auto res = emit(*stmt); !res)
      return std::unexpected(res.error());
  }

  return {};
}

std::expected<void, Error> IrGen::emit(Stmt const &stmt) {
  auto vtor = Overloaded{ /* Formatter fix */
    [&](DimStmt const &s) -> std::expected<void, Error> {
      auto type = resolve_type(s.type);
      if (!type)
        return std::unexpected(Error{ std::format("Unknown type '{}'", s.type),
          stmt.range.start.line,
          stmt.range.start.col });

      if (auto res = emit(*s.init); !res)
        return std::unexpected(res.error());

      auto var = var_register(s.name);
      prog.push_back({ Instruction::DefineVar, var });
      prog.push_back({ Instruction::Extension, *type });
      prog.push_back({ Instruction::StoreVar, var });

      return {};
    },
    [&](LetStmt const &s) -> std::expected<void, Error> {
      if (auto res = emit(*s.value); !res)
        return std::unexpected(res.error());

      auto var = var_register(s.name);
      prog.push_back({ Instruction::StoreVar, var });
      return {};
    },
    [&](IfStmt const &s) -> std::expected<void, Error> {
      auto l_else = label_get();
      auto l_end = label_get();

      if (auto res = emit(*s.condition); !res)
        return std::unexpected(res.error());

      prog.push_back({ Instruction::JumpFalse, l_else });

      for (auto const &stmt : s.then_branch) {
        if (auto res = emit(*stmt); !res)
          return std::unexpected(res.error());
      }
      prog.push_back({ Instruction::Goto, l_end });

      prog.push_back({ Instruction::Label, l_else });

      auto else_res = std::visit(
        Overloaded{
          [&](std::monostate) -> std::expected<void, Error> { return {}; },
          [&](
            std::vector<StmtPtr> const &branch) -> std::expected<void, Error> {
            for (auto const &stmt : branch) {
              if (auto res = emit(*stmt); !res)
                return std::unexpected(res.error());
            }
            return {};
          },
          [&](StmtPtr const &branch) -> std::expected<void, Error> {
            if (auto res = emit(*branch); !res)
              return std::unexpected(res.error());
            return {};
          } },
        s.else_branch);

      if (!else_res)
        return std::unexpected(else_res.error());

      prog.push_back({ Instruction::Label, l_end });

      return {};
    },
    [&](ForStmt const &s) -> std::expected<void, Error> {
      auto var = var_register(s.var);
      if (auto res = emit(*s.start); !res)
        return std::unexpected(res.error());

      prog.push_back({ Instruction::StoreVar, var });
      auto l_start = label_get();
      prog.push_back({ Instruction::Label, l_start });

      for (auto const &stmt : s.body) {
        if (auto res = emit(*stmt); !res)
          return std::unexpected(res.error());
      }

      if (s.step) {
        prog.push_back({ Instruction::LoadVar, var });
        if (auto res = emit(*s.step); !res)
          return std::unexpected(res.error());
        prog.push_back({ Instruction::Add });
        prog.push_back({ Instruction::StoreVar, var });
      } else {
        auto c = const_register({ PeaNumber{ 1 } });
        prog.push_back({ Instruction::LoadVar, var });
        prog.push_back({ Instruction::LoadConst, c });
        prog.push_back({ Instruction::Add });
        prog.push_back({ Instruction::StoreVar, var });
      }

      prog.push_back({ Instruction::LoadVar, var });
      if (auto res = emit(*s.end); !res)
        return std::unexpected(res.error());
      prog.push_back({ Instruction::LessThanEq });
      prog.push_back({ Instruction::JumpTrue, l_start });

      return {};
    },
    [&](GotoStmt const &s) -> std::expected<void, Error> {
      auto l = label_register(s.label);
      prog.push_back({ Instruction::Goto, l });
      return {};
    },
    [&](DoStmt const &s) -> std::expected<void, Error> {
      if (s.while_at_start) {
        auto l_start = label_get();
        prog.push_back({ Instruction::Label, l_start });
        if (auto res = emit(*s.condition); !res)
          return std::unexpected(res.error());
        auto l_end = label_get();
        prog.push_back({ Instruction::JumpFalse, l_end });
        for (auto const &stmt : s.body) {
          if (auto res = emit(*stmt); !res)
            return std::unexpected(res.error());
        }
        prog.push_back({ Instruction::Goto, l_start });
        prog.push_back({ Instruction::Label, l_end });
      } else {
        auto l_start = label_get();
        prog.push_back({ Instruction::Label, l_start });
        for (auto const &stmt : s.body) {
          if (auto res = emit(*stmt); !res)
            return std::unexpected(res.error());
        }
        if (auto res = emit(*s.condition); !res)
          return std::unexpected(res.error());
        prog.push_back({ Instruction::JumpTrue, l_start });
      }
      return {};
    },
    [&](ExprStmt const &s) -> std::expected<void, Error> {
      if (auto res = emit(*s.expr); !res)
        return std::unexpected(res.error());
      prog.push_back({ Instruction::Pop });
      return {};
    },
    [&](LabelStmt const &s) -> std::expected<void, Error> {
      auto l = label_register(s.name);
      prog.push_back({ Instruction::Label, l });
      return {};
    },
    [&](SubDecl const &s) -> std::expected<void, Error> {
      return {};
    },
    [&](ReturnStmt const &s) -> std::expected<void, Error> {
      return {};
    },
    [&](BreakStmt const &s) -> std::expected<void, Error> {
      return {};
    },
    [&](ContinueStmt const &s) -> std::expected<void, Error> {
      return {};
    }
  };
  return std::visit(vtor, stmt.data);
}

std::expected<void, Error> IrGen::emit(Expr const &expr) {
  auto vtor = Overloaded{ /* Formatter fix */
    [&](Literal const &e) -> std::expected<void, Error> {
      auto val =
        std::visit(Overloaded{ /* Formatter fix */
                     [](double x) -> PeaValue { return { PeaNumber{ x } }; },
                     [](char x) -> PeaValue { return { PeaChar{ x } }; },
                     [](std::string x) -> PeaValue {
                       return { PeaString{ std::move(x) } };
                     } },
          e.value);
      auto c = const_register(val);
      prog.push_back({ Instruction::LoadConst, c });
      return {};
    },
    [&](Variable const &e) -> std::expected<void, Error> {
      std::uint16_t v = var_register(e.name);
      prog.push_back({ Instruction::LoadVar, v });
      return {};
    },
    [&](BinaryOp const &e) -> std::expected<void, Error> {
      if (auto res = emit(*e.left); !res)
        return std::unexpected(res.error());

      if (auto res = emit(*e.right); !res)
        return std::unexpected(res.error());

      switch (e.op) {
      case TokenType::KW_Mod:
        break;
      case TokenType::KW_And:
        break;
      case TokenType::KW_Or:
        break;
      case TokenType::Plus:
        prog.push_back({ Instruction::Add });
        break;
      case TokenType::Minus:
        break;
      case TokenType::Star:
        break;
      case TokenType::Slash:
        break;
      case TokenType::Caret:
        break;
      case TokenType::Backslash:
        break;
      case TokenType::Eq:
        break;
      case TokenType::Neq:
        break;
      case TokenType::Less:
        break;
      case TokenType::Great:
        break;
      case TokenType::Lteq:
        break;
      case TokenType::Gteq:
        break;
      case TokenType::Amp:
        break;
      default:
        std::unreachable();
      }

      return {};
    },
    [&](UnaryOp const &e) -> std::expected<void, Error> {
      if (auto res = emit(*e.operand); !res)
        return std::unexpected(res.error());

      switch (e.op) {
      case TokenType::Plus:
        break;
      case TokenType::Minus:
        break;
      case TokenType::KW_Not:
        break;
      default:
        std::unreachable();
      }

      return {};
    },
    [&](Call const &e) -> std::expected<void, Error> { return {}; }
  };
  return std::visit(vtor, expr.data);
}

std::uint16_t IrGen::var_register(std::string const &name) {
  if (auto it = variables.find(name); it != variables.end()) {
    return it->second;
  } else {
    std::uint16_t id = var_next++;
    variables.insert({ name, id });
    return id;
  }
}

std::optional<std::uint16_t> IrGen::var_get(std::string const &name) {
  if (auto it = variables.find(name); it != variables.end()) {
    return it->second;
  } else {
    return {};
  }
}

std::uint16_t IrGen::const_register(PeaValue val) {
  consts.push_back(std::move(val));
  return static_cast<std::uint16_t>(consts.size() - 1);
}

std::uint16_t IrGen::label_register(std::string lbl) {
  if (auto it = labels.find(lbl); it != labels.end()) {
    return it->second;
  } else {
    std::uint16_t id = label_next++;
    labels.insert({ lbl, id });
    return id;
  }
}

std::optional<std::uint16_t> IrGen::resolve_type(std::string const &name) {
  if (auto it = types.find(name); it != types.end()) {
    return it->second;
  } else {
    return {};
  }
}

std::uint16_t IrGen::label_get() {
  std::uint16_t id = label_next++;
  std::string name{ "$LABEL" + std::to_string(id) };
  labels.insert({ name, id });
  return id;
}

std::ostream &operator<<(std::ostream &os, ProgramIr const &ir) {
  for (auto it = ir.prog.begin(); it != ir.prog.end(); ++it) {
    auto const &instr = *it;
    switch (instr.kind) {
    case Instruction::Add:
      os << "ADD\n";
      break;
    case Instruction::LessThanEq:
      os << "LESS_THAN_EQ\n";
      break;
    case Instruction::LoadVar: {
      auto name = std::find_if(ir.vars.begin(),
        ir.vars.end(),
        [&instr](auto const &p) { return p.second == instr.data; });
      os << "LOAD_VAR " << instr.data << "[" << name->first << "]\n";
    } break;
    case Instruction::DefineVar: {
      auto name = std::find_if(ir.vars.begin(),
        ir.vars.end(),
        [&instr](auto const &p) { return p.second == instr.data; });
      auto const &ext = *(++it);
      auto type = std::find_if(ir.types.begin(),
        ir.types.end(),
        [&ext](auto const &p) { return p.second == ext.data; });
      os << "DEFINE_VAR " << instr.data << "[" << name->first << "], "
         << ext.data << "[" << type->first << "]\n";
    } break;
    case Instruction::StoreVar: {
      auto name = std::find_if(ir.vars.begin(),
        ir.vars.end(),
        [&instr](auto const &p) { return p.second == instr.data; });
      os << "STORE_VAR " << instr.data << "[" << name->first << "]\n";
    } break;
    case Instruction::LoadConst: {
      os << "LOAD_CONST " << instr.data << "[";
      std::visit([&os](auto const &v) { os << v.val << "]\n"; },
        ir.consts[instr.data].data);
    } break;
    case Instruction::Pop: {
      os << "POP\n";
    } break;
    case Instruction::Label: {
      auto name = std::find_if(ir.labels.begin(),
        ir.labels.end(),
        [&instr](auto const &p) { return p.second == instr.data; });
      os << "LABEL " << instr.data << "[" << name->first << "]\n";
    } break;
    case Instruction::Goto: {
      auto name = std::find_if(ir.labels.begin(),
        ir.labels.end(),
        [&instr](auto const &p) { return p.second == instr.data; });
      os << "GOTO " << instr.data << "[" << name->first << "]\n";
    } break;
    case Instruction::JumpFalse: {
      auto name = std::find_if(ir.labels.begin(),
        ir.labels.end(),
        [&instr](auto const &p) { return p.second == instr.data; });
      os << "JUMP_FALSE " << instr.data << "[" << name->first << "]\n";
    } break;
    case Instruction::JumpTrue: {
      auto name = std::find_if(ir.labels.begin(),
        ir.labels.end(),
        [&instr](auto const &p) { return p.second == instr.data; });
      os << "JUMP_TRUE " << instr.data << "[" << name->first << "]\n";
    } break;
    case Instruction::Extension:
      break;
    }
  }

  return os;
}
