#pragma once
#include "ast.hpp"
#include "lexer.hpp"
#include <cstdint>
#include <expected>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct ConstNumber {
  double val;
};
struct ConstChar {
  char val;
};
struct ConstString {
  std::string val;
};
struct ConstFuncPtr {
  std::uint16_t id;
};
struct ConstClassPtr {
  std::uint16_t id;
};

struct ConstData {
  std::variant<ConstNumber, ConstChar, ConstString, ConstFuncPtr, ConstClassPtr> data;
  std::uint16_t id;
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

struct ClassData {
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
  
  std::string name;
  std::uint16_t id;
};

struct FuncData {
  std::vector<Instruction> body;
  std::string name;
  std::uint16_t id;
};

struct ProgramIr {
  std::vector<Instruction> prog;
  std::unordered_map<std::string, std::uint16_t> vars;
  std::vector<ConstData> consts;
  std::unordered_map<std::string, std::uint16_t> labels;
  std::vector<FuncData> funcs;
  std::vector<ClassData> classes;
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
  PEA_ID_DIM = (1 << 16) - 9,
  PEA_ID_COPY = (1 << 16) - 10
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
    { "dims", PEA_ID_DIM },
    { "copy", PEA_ID_COPY }
  };
  std::uint16_t var_next{};
  std::uint16_t var_register(std::string const &name);
  std::uint16_t var_fresh();

  std::vector<ConstData> consts{};
  std::uint16_t const_next{};

  std::unordered_map<std::string, std::uint16_t> labels{};
  std::uint16_t label_next{};
  std::uint16_t label_register(std::string lbl);
  std::uint16_t label_get();

  std::vector<LoopLabels> loop_stack{};

  std::vector<FuncData> funcs{};
  std::uint16_t func_next{};

  std::vector<ClassData> classes{};
  std::uint16_t class_next{};

  std::expected<void, Error> sub_emit(SubDecl const &s, std::size_t index);
};
