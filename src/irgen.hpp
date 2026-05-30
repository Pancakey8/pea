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
struct PeaClassPtr {
  std::uint16_t id;
};

struct PeaValue {
  std::variant<PeaNumber, PeaChar, PeaString, PeaFuncPtr, PeaClassPtr> data;
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
    DefineVar,
    StoreVar,
    Deref,
    Member,
    Dispatch,
    Construct,
    LoadConst,
    Pop,
    Label,
    Goto,
    JumpFalse,
    JumpTrue,
    Call,
    Return,
    StoreVField,
    Extension = 0xFF
  } kind;
  std::uint16_t data;
};

struct ClassTable {
  struct Field {
    std::uint16_t name;
    bool is_public;
    bool is_static;
  };

  struct Method {
    std::uint16_t name;
    std::uint16_t id;
    bool is_public;
    bool is_static;
  };

  std::vector<Field> fields;
  std::vector<Method> methods;
};

struct ProgramIr {
  std::vector<Instruction> prog;
  std::unordered_map<std::string, std::uint16_t> vars;
  std::vector<PeaValue> consts;
  std::unordered_map<std::string, std::uint16_t> labels;
  std::vector<std::string> func_names;
  std::vector<std::vector<Instruction>> func_bodies;
  std::vector<std::string> class_names;
  std::vector<ClassTable> classes;
};

std::ostream &operator<<(std::ostream &os, ProgramIr const &ir);

struct LoopLabels {
  std::uint16_t continue_lbl;
  std::uint16_t break_lbl;
};

enum PeaBuiltinIdentifier {
  PEA_ID_LENGTH = (1 << 16) - 1,
  PEA_ID_AT = (1 << 16) - 2,
  PEA_ID_PRINTLN = (1 << 16) - 3,
  PEA_ID_TONUM = (1 << 16) - 4,
  PEA_ID_TOSTRING = (1 << 16) - 5,
  PEA_ID_ISTRUTHY = (1 << 16) - 6,
  PEA_ID_EQUALS = (1 << 16) - 7,
  PEA_ID_THIS = (1 << 16) - 8,
  PEA_ID_DIM = (1 << 16) - 9
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
    { "length", PEA_ID_LENGTH },
    { "at", PEA_ID_AT },
    { "println", PEA_ID_PRINTLN },
    { "tonum", PEA_ID_TONUM },
    { "tostring", PEA_ID_TOSTRING },
    { "istruthy", PEA_ID_ISTRUTHY },
    { "equals", PEA_ID_EQUALS },
    { "this", PEA_ID_THIS },
    { "dims", PEA_ID_DIM }
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

  std::vector<LoopLabels> loop_stack{};

  std::vector<std::string> func_names{};
  std::vector<std::vector<Instruction>> func_bodies{};
  std::uint16_t func_next{};

  std::vector<std::string> class_names{};
  std::vector<ClassTable> classes{};
  std::uint16_t class_next{};
  std::uint16_t class_register(std::string lbl);

  std::expected<void, Error> sub_emit(SubDecl const &s, std::uint16_t id);
};
