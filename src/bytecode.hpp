#pragma once
#include "irgen.hpp"
#include <cstdint>
#include <vector>

static_assert(sizeof(double) == 8, "Must have 64-bit doubles at this stage");

enum class OpCode : std::uint8_t {
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
  Deref,
  Member,
  Dispatch,
  LoadVar,
  LoadRef,
  DefineVar,
  DefineVarT,
  StoreVar,
  StoreVarI,
  LoadConst,
  LoadNull,
  Pop,
  Goto,
  JumpFalse,
  JumpTrue,
  Call,
  Return,
  Extension = 0xFF
};

class BytecodeEmitter {
public:
  explicit BytecodeEmitter(ProgramIr const &prog);

  void emit(std::vector<std::uint8_t> &out);

private:
  void emit_consts(std::vector<std::uint8_t> &out);
  void emit_subs(std::vector<std::uint8_t> &out);
  void emit_body(
    std::vector<Instruction> const &instrs, std::vector<std::uint8_t> &out);
  void resolve_ids(std::vector<std::uint8_t> &bytes);

  std::vector<std::size_t> consts{};
  std::vector<std::size_t> resolve_consts{};

  std::vector<std::size_t> subs{};
  std::vector<std::size_t> resolve_subs{};

  std::vector<std::size_t> labels{};
  std::vector<std::size_t> resolve_labels{};

  ProgramIr const &prog;
};
