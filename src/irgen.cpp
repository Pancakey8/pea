#include "irgen.hpp"
#include "ast.hpp"
#include "lexer.hpp"
#include <iostream>
#include <ranges>
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
    std::move(func_names),
    std::move(func_bodies),
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
      for (auto const &dim : s.dims) {
        if (auto res = emit(*dim); !res)
          return std::unexpected(res.error());
      }

      auto var = var_register(s.name);
      prog.push_back({ Instruction::DefineVar, var });
      prog.push_back(
        { Instruction::Extension, static_cast<std::uint16_t>(s.dims.size()) });

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
      auto l_continue = label_get();
      auto l_end = label_get();
      prog.push_back({ Instruction::Label, l_start });

      loop_stack.push_back({ l_continue, l_end });
      for (auto const &stmt : s.body) {
        if (auto res = emit(*stmt); !res)
          return std::unexpected(res.error());
      }
      loop_stack.pop_back();

      prog.push_back({ Instruction::Label, l_continue });
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
      prog.push_back({ Instruction::Lte });
      prog.push_back({ Instruction::JumpTrue, l_start });
      prog.push_back({ Instruction::Label, l_end });

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
        auto l_end = label_get();
        prog.push_back({ Instruction::Label, l_start });
        if (auto res = emit(*s.condition); !res)
          return std::unexpected(res.error());
        prog.push_back({ Instruction::JumpFalse, l_end });

        loop_stack.push_back({ l_start, l_end });
        for (auto const &stmt : s.body) {
          if (auto res = emit(*stmt); !res)
            return std::unexpected(res.error());
        }
        loop_stack.pop_back();

        prog.push_back({ Instruction::Goto, l_start });
        prog.push_back({ Instruction::Label, l_end });
      } else {
        auto l_start = label_get();
        auto l_continue = label_get();
        auto l_end = label_get();
        prog.push_back({ Instruction::Label, l_start });

        loop_stack.push_back({ l_continue, l_end });
        for (auto const &stmt : s.body) {
          if (auto res = emit(*stmt); !res)
            return std::unexpected(res.error());
        }
        loop_stack.pop_back();

        prog.push_back({ Instruction::Label, l_continue });
        if (auto res = emit(*s.condition); !res)
          return std::unexpected(res.error());
        prog.push_back({ Instruction::JumpTrue, l_start });
        prog.push_back({ Instruction::Label, l_end });
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
      std::uint16_t id = func_next++;
      func_names.push_back(s.name);
      func_bodies.push_back({});

      auto c = const_register(PeaValue{ PeaFuncPtr{ id } });
      prog.push_back({ Instruction::LoadConst, c });
      auto v = var_register(s.name);
      prog.push_back({ Instruction::StoreVar, v });

      std::vector<Instruction> main_prog = std::move(prog);
      prog.clear();

      for (auto const &param : s.params | std::ranges::views::reverse) {
        auto type = resolve_type(param.type);
        if (!type)
          return std::unexpected(
            Error{ std::format("Unknown type '{}'", param.type),
              stmt.range.start.line,
              stmt.range.start.col });

        auto var = var_register(param.name);
        prog.push_back({ Instruction::DefineVar, var });
        prog.push_back({ Instruction::Extension, *type });
        prog.push_back({ Instruction::StoreVar, var });
      }

      for (auto const &stmt : s.body) {
        if (auto res = emit(*stmt); !res) {
          prog = std::move(main_prog);
          return std::unexpected(res.error());
        }
      }

      prog.push_back({ Instruction::Return, 0 });

      func_bodies[id] = std::move(prog);
      prog = std::move(main_prog);

      return {};
    },
    [&](ReturnStmt const &s) -> std::expected<void, Error> {
      if (s.value) {
        if (auto res = emit(*s.value); !res)
          return std::unexpected(res.error());
      }
      prog.push_back(
        { Instruction::Return, static_cast<uint16_t>(s.value ? 1 : 0) });
      return {};
    },
    [&](BreakStmt const &s) -> std::expected<void, Error> {
      prog.push_back({ Instruction::Goto, loop_stack.back().break_lbl });
      return {};
    },
    [&](ContinueStmt const &s) -> std::expected<void, Error> {
      prog.push_back({ Instruction::Goto, loop_stack.back().continue_lbl });
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
        prog.push_back({ Instruction::Mod });
        break;
      case TokenType::KW_And:
        prog.push_back({ Instruction::And });
        break;
      case TokenType::KW_Or:
        prog.push_back({ Instruction::Or });
        break;
      case TokenType::Plus:
        prog.push_back({ Instruction::Add });
        break;
      case TokenType::Minus:
        prog.push_back({ Instruction::Sub });
        break;
      case TokenType::Star:
        prog.push_back({ Instruction::Mul });
        break;
      case TokenType::Slash:
        prog.push_back({ Instruction::Div });
        break;
      case TokenType::Caret:
        prog.push_back({ Instruction::Pow });
        break;
      case TokenType::Backslash:
        prog.push_back({ Instruction::IDiv });
        break;
      case TokenType::Eq:
        prog.push_back({ Instruction::Eq });
        break;
      case TokenType::Neq:
        prog.push_back({ Instruction::Neq });
        break;
      case TokenType::Less:
        prog.push_back({ Instruction::Lt });
        break;
      case TokenType::Great:
        prog.push_back({ Instruction::Gt });
        break;
      case TokenType::Lteq:
        prog.push_back({ Instruction::Lte });
        break;
      case TokenType::Gteq:
        prog.push_back({ Instruction::Gte });
        break;
      case TokenType::Amp:
        prog.push_back({ Instruction::Concat });
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
        prog.push_back({ Instruction::Pos });
        break;
      case TokenType::Minus:
        prog.push_back({ Instruction::Neg });
        break;
      case TokenType::KW_Not:
        prog.push_back({ Instruction::Not });
        break;
      default:
        std::unreachable();
      }

      return {};
    },
    [&](Call const &e) -> std::expected<void, Error> {
      for (auto const &arg : e.args) {
        if (auto res = emit(*arg); !res)
          return std::unexpected(res.error());
      }
      if (auto res = emit(*e.callee); !res)
        return std::unexpected(res.error());
      prog.push_back(
        { Instruction::Call, static_cast<uint16_t>(e.args.size()) });
      return {};
    }
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

template <typename It>
void print_instr(std::ostream &os, It &it, ProgramIr const &ir) {
  auto const &instr = *it;
  switch (instr.kind) {
  case Instruction::Add:
    os << "ADD\n";
    break;
  case Instruction::Sub:
    os << "SUB\n";
    break;
  case Instruction::Mul:
    os << "MUL\n";
    break;
  case Instruction::Div:
    os << "DIV\n";
    break;
  case Instruction::IDiv:
    os << "IDIV\n";
    break;
  case Instruction::Pow:
    os << "POW\n";
    break;
  case Instruction::Mod:
    os << "MOD\n";
    break;
  case Instruction::And:
    os << "AND\n";
    break;
  case Instruction::Or:
    os << "OR\n";
    break;
  case Instruction::Eq:
    os << "EQ\n";
    break;
  case Instruction::Neq:
    os << "NEQ\n";
    break;
  case Instruction::Lt:
    os << "LT\n";
    break;
  case Instruction::Gt:
    os << "GT\n";
    break;
  case Instruction::Lte:
    os << "LTE\n";
    break;
  case Instruction::Gte:
    os << "GTE\n";
    break;
  case Instruction::Concat:
    os << "CONCAT\n";
    break;
  case Instruction::Pos:
    os << "POS\n";
    break;
  case Instruction::Neg:
    os << "NEG\n";
    break;
  case Instruction::Not:
    os << "NOT\n";
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
    os << "DEFINE_VAR " << instr.data << "[" << name->first << "], " << ext.data << "\n";
  } break;
  case Instruction::StoreVar: {
    auto name = std::find_if(ir.vars.begin(),
      ir.vars.end(),
      [&instr](auto const &p) { return p.second == instr.data; });
    os << "STORE_VAR " << instr.data << "[" << name->first << "]\n";
  } break;
  case Instruction::LoadConst: {
    os << "LOAD_CONST " << instr.data << "[";
    std::visit(
      [&os](auto const &v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, PeaNumber>)
          os << v.val;
        else if constexpr (std::is_same_v<T, PeaChar>)
          os << v.val;
        else if constexpr (std::is_same_v<T, PeaString>)
          os << v.val;
        else if constexpr (std::is_same_v<T, PeaFuncPtr>)
          os << "func_ptr(" << v.id << ")";
        os << "]\n";
      },
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
  case Instruction::Call:
    os << "CALL " << instr.data << "\n";
    break;
  case Instruction::Return:
    os << "RETURN " << instr.data << "\n";
    break;
  case Instruction::Extension:
    break;
  }
}

std::ostream &operator<<(std::ostream &os, ProgramIr const &ir) {
  for (auto it = ir.prog.begin(); it != ir.prog.end(); ++it) {
    print_instr(os, it, ir);
  }

  if (!ir.func_names.empty()) {
    os << "\n--- Functions ---\n";
    for (size_t i = 0; i < ir.func_names.size(); ++i) {
      os << "FUNC " << i << " [" << ir.func_names[i] << "]:\n";
      for (auto it = ir.func_bodies[i].begin(); it != ir.func_bodies[i].end();
        ++it) {
        os << "  ";
        print_instr(os, it, ir);
      }
    }
  }

  return os;
}
