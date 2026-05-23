#pragma once
#include "ast.hpp"
#include "lexer.hpp"
#include <cstdint>
#include <expected>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct PeaNumber {
  double val;
};
struct PeaChar {
  char val;
};
struct PeaString {
  std::string val;
};
struct PeaFuncPtr {
  std::uint16_t id;
};

struct PeaValue {
  std::variant<PeaNumber, PeaChar, PeaString, PeaFuncPtr> data;
};

struct Instruction {
  enum : std::uint8_t {
    Add,
    Sub,
    Mul,
    Div,
    IDiv,
    Pow,
    Mod,
    And,
    Or,
    Eq,
    Neq,
    Lt,
    Gt,
    Lte,
    Gte,
    Concat,
    Pos,
    Neg,
    Not,
    LoadVar,
    LoadRef,
    DefineVar,
    StoreVar,
    Deref,
    Member,
    Dispatch,
    LoadConst,
    Pop,
    Label,
    Goto,
    JumpFalse,
    JumpTrue,
    Call,
    Return,
    Extension = 0xFF
  } kind;
  std::uint16_t data;
};

struct ProgramIr {
  std::vector<Instruction> prog;
  std::unordered_map<std::string, std::uint16_t> vars;
  std::vector<PeaValue> consts;
  std::unordered_map<std::string, std::uint16_t> labels;
  std::unordered_map<std::string, std::uint16_t> types;
  std::vector<std::string> func_names;
  std::vector<std::vector<Instruction>> func_bodies;
};

std::ostream &operator<<(std::ostream &os, ProgramIr const &ir);

struct LoopLabels {
  std::uint16_t continue_lbl;
  std::uint16_t break_lbl;
};

enum PeaBuiltinIdentifier {
  PEA_ID_LENGTH = (1 << 16) - 1,
  PEA_ID_AT = (1 << 16) - 2
};

class IrGen {
public:
  std::expected<ProgramIr, Error> generate(Program const &prog);

private:
  std::expected<void, Error> emit(Program const &prog);
  std::expected<void, Error> emit(Stmt const &stmt);
  std::expected<void, Error> emit(Expr const &expr);

  std::vector<Instruction> prog;

  std::unordered_map<std::string, std::uint16_t> variables{
    {"length", PEA_ID_LENGTH},
    {"at", PEA_ID_AT}
  };
  std::uint16_t var_next{};
  std::uint16_t var_register(std::string const &name);
  std::optional<std::uint16_t> var_get(std::string const &name);
  std::uint16_t var_fresh();

  std::vector<PeaValue> consts{};
  std::uint16_t const_register(PeaValue val);

  std::unordered_map<std::string, std::uint16_t> labels{};
  std::uint16_t label_next{};
  std::uint16_t label_register(std::string lbl);
  std::uint16_t label_get();

  std::unordered_map<std::string, std::uint16_t> types{
    { "number", 0 }, { "char", 1 }, { "string", 2}, {"array", 3}
  };
  std::optional<std::uint16_t> resolve_type(std::string const &name);

  std::vector<LoopLabels> loop_stack{};

  std::vector<std::string> func_names{};
  std::vector<std::vector<Instruction>> func_bodies{};
  std::uint16_t func_next{};
};
