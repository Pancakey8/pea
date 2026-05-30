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
  DefineVar,
  StoreVar,
  LoadConst,
  LoadNull,
  Pop,
  Goto,
  JumpFalse,
  JumpTrue,
  Call,
  Return,
  Construct,
  StoreVField,
  ProgPrefix = 0xFE,
  Extension = 0xFF
};

class BytecodeEmitter {
public:
  void emit(ProgramIr const &prog, std::vector<std::uint8_t> &out);

private:
  void emit_consts(ProgramIr const &prog, std::vector<std::uint8_t> &out);
  void emit_subs(ProgramIr const &prog, std::vector<std::uint8_t> &out);
  void emit_classes(ProgramIr const &prog, std::vector<std::uint8_t> &out);
  void emit_body(
    std::vector<Instruction> const &instrs, std::vector<std::uint8_t> &out);
  void resolve_ids(ProgramIr const &prog, std::vector<std::uint8_t> &bytes);

  std::size_t global_offset{};

  std::vector<std::size_t> consts{};
  std::vector<std::size_t> resolve_consts{};

  std::vector<std::size_t> subs{};
  std::vector<std::size_t> resolve_subs{};

  std::vector<std::size_t> labels{};
  std::vector<std::size_t> resolve_labels{};
};
