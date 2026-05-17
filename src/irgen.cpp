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
        return std::unexpected(
          Error{ std::format("Unknown type '{}'", s.type), 0, 0 });

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
      return {};
    },
    [&](ForStmt const &s) -> std::expected<void, Error> { return {}; },
    [&](GotoStmt const &s) -> std::expected<void, Error> {
      auto l = label_register(s.label);
      prog.push_back({ Instruction::Goto, l });
      return {};
    },
    [&](DoStmt const &s) -> std::expected<void, Error> { return {}; },
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
  std::string name{"$LABEL" + std::to_string(id)};
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
    case Instruction::Extension:
      break;
    }
  }

  return os;
}
