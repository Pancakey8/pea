#include "bytecode.hpp"
#include "irgen.hpp"
#include <bit>
#include <cassert>
#include <ranges>

template <class... Fs> struct Overloaded : Fs... {
  using Fs::operator()...;
};

template <typename Int, typename Iter>
void emit_le(std::vector<std::uint8_t> &out, Iter targ, Int num) {
  auto bytes = std::bit_cast<std::array<std::uint8_t, sizeof(Int)>>(num);

  if constexpr (std::endian::native == std::endian::big) {
    std::reverse(bytes.begin(), bytes.end());
  }

  out.insert(targ, bytes.begin(), bytes.end());
}

template <typename Int>
void write_le(std::vector<std::uint8_t> &out, std::size_t offset, Int num) {
  auto bytes = std::bit_cast<std::array<std::uint8_t, sizeof(Int)>>(num);

  if constexpr (std::endian::native == std::endian::big) {
    std::reverse(bytes.begin(), bytes.end());
  }

  std::copy(bytes.begin(), bytes.end(), out.begin() + offset);
}

void BytecodeEmitter::emit(
  ProgramIr const &prog, std::vector<std::uint8_t> &out) {
  labels.resize(prog.labels.size());

  out.insert(out.end(),
    { static_cast<unsigned char>('P'),
      static_cast<unsigned char>('E'),
      static_cast<unsigned char>('A') });
  emit_consts(prog, out);
  emit_subs(prog, out);
  emit_classes(prog, out);
  emit_body(prog.prog, out);
  resolve_ids(prog, out);
  global_offset += out.size();
}

// <table_size:8><const_count:2><consts...:?>
void BytecodeEmitter::emit_consts(
  ProgramIr const &prog, std::vector<std::uint8_t> &out) {

  auto head_start = out.size();
  out.resize(out.size() + sizeof(std::size_t) + sizeof(std::uint16_t));
  auto head_end = out.size();
  auto table_size = head_start;
  auto const_count = table_size + sizeof(std::size_t);

  for (auto const &c : prog.consts) {
    emit_le(out, out.end(), static_cast<std::uint16_t>(c.id));
    std::visit(
      Overloaded{ [&](ConstNumber const &v) {
                   out.push_back(static_cast<std::uint8_t>(ConstKind::Number));
                   emit_le(out, out.end(), std::bit_cast<std::uint64_t>(v.val));
                 },
        [&](ConstChar const &v) {
          out.push_back(static_cast<std::uint8_t>(ConstKind::Char));
          emit_le(out, out.end(), static_cast<std::uint8_t>(v.val));
        },
        [&](ConstString const &v) {
          out.push_back(static_cast<std::uint8_t>(ConstKind::String));
          emit_le(out, out.end(), static_cast<std::uint64_t>(v.val.size()));
          out.insert(out.end(),
            reinterpret_cast<const unsigned char *>(v.val.data()),
            reinterpret_cast<const unsigned char *>(
              v.val.data() + v.val.size()));
        },
        [&](ConstFuncPtr const &v) {
          out.push_back(static_cast<std::uint8_t>(ConstKind::FuncPtr));
          emit_le(out, out.end(), static_cast<std::uint16_t>(v.id));
        },
        [&](ConstClassPtr const &v) {
          out.push_back(static_cast<std::uint8_t>(ConstKind::ClassPtr));
          emit_le(out, out.end(), static_cast<std::uint16_t>(v.id));
        } },
      c.data);
  }

  write_le(
    out, table_size, static_cast<std::uint64_t>(out.size() - head_start - 8));
  write_le(out, const_count, static_cast<std::uint16_t>(prog.consts.size()));
}

void BytecodeEmitter::emit_body(
  std::vector<Instruction> const &instrs, std::vector<std::uint8_t> &out) {
  auto body_size = out.size();
  out.resize(out.size() + sizeof(std::size_t));

  auto body_start = out.size();
  for (auto it = instrs.begin(); it != instrs.end(); ++it) {
    auto const &instr = *it;
    switch (instr.kind) {
    case Instruction::Add:
      out.push_back(static_cast<std::uint8_t>(OpCode::Add));
      break;
    case Instruction::Sub:
      out.push_back(static_cast<std::uint8_t>(OpCode::Sub));
      break;
    case Instruction::Mul:
      out.push_back(static_cast<std::uint8_t>(OpCode::Mul));
      break;
    case Instruction::Div:
      out.push_back(static_cast<std::uint8_t>(OpCode::Div));
      break;
    case Instruction::IDiv:
      out.push_back(static_cast<std::uint8_t>(OpCode::IDiv));
      break;
    case Instruction::Pow:
      out.push_back(static_cast<std::uint8_t>(OpCode::Pow));
      break;
    case Instruction::Mod:
      out.push_back(static_cast<std::uint8_t>(OpCode::Mod));
      break;
    case Instruction::And:
      out.push_back(static_cast<std::uint8_t>(OpCode::And));
      break;
    case Instruction::Or:
      out.push_back(static_cast<std::uint8_t>(OpCode::Or));
      break;
    case Instruction::Eq:
      out.push_back(static_cast<std::uint8_t>(OpCode::Eq));
      break;
    case Instruction::Neq:
      out.push_back(static_cast<std::uint8_t>(OpCode::Neq));
      break;
    case Instruction::Lt:
      out.push_back(static_cast<std::uint8_t>(OpCode::Lt));
      break;
    case Instruction::Gt:
      out.push_back(static_cast<std::uint8_t>(OpCode::Gt));
      break;
    case Instruction::Lte:
      out.push_back(static_cast<std::uint8_t>(OpCode::Lte));
      break;
    case Instruction::Gte:
      out.push_back(static_cast<std::uint8_t>(OpCode::Gte));
      break;
    case Instruction::Concat:
      out.push_back(static_cast<std::uint8_t>(OpCode::Concat));
      break;
    case Instruction::Pos:
      out.push_back(static_cast<std::uint8_t>(OpCode::Pos));
      break;
    case Instruction::Neg:
      out.push_back(static_cast<std::uint8_t>(OpCode::Neg));
      break;
    case Instruction::Not:
      out.push_back(static_cast<std::uint8_t>(OpCode::Not));
      break;
    case Instruction::Deref:
      out.push_back(static_cast<std::uint8_t>(OpCode::Deref));
      break;
    case Instruction::Member:
      out.push_back(static_cast<std::uint8_t>(OpCode::Member));
      emit_le(out, out.end(), static_cast<std::uint16_t>(instr.data));
      break;
    case Instruction::LoadVar:
      out.push_back(static_cast<std::uint8_t>(OpCode::LoadVar));
      emit_le(out, out.end(), static_cast<std::uint16_t>(instr.data));
      break;
    case Instruction::DefineVar: {
      out.push_back(static_cast<std::uint8_t>(OpCode::DefineVar));
      emit_le(out, out.end(), static_cast<std::uint16_t>(instr.data));
      auto dim_ext = *(++it);
      emit_le(out, out.end(), static_cast<std::uint16_t>(dim_ext.data));
    } break;
    case Instruction::StoreVar: {
      auto op = out.size();
      out.push_back(static_cast<std::uint8_t>(OpCode::StoreVar));
    } break;
    case Instruction::LoadConst:
      out.push_back(static_cast<std::uint8_t>(OpCode::LoadConst));
      emit_le(out, out.end(), static_cast<std::uint16_t>(instr.data));
      break;
    case Instruction::Pop:
      out.push_back(static_cast<std::uint8_t>(OpCode::Pop));
      break;
    case Instruction::Label:
      labels[instr.data] = global_offset + out.size();
      break;
    case Instruction::Goto:
      out.push_back(static_cast<std::uint8_t>(OpCode::Goto));
      resolve_labels.push_back(out.size());
      emit_le(out, out.end(), static_cast<std::uint16_t>(instr.data));
      out.resize(out.size() + 2);
      break;
    case Instruction::JumpFalse:
      out.push_back(static_cast<std::uint8_t>(OpCode::JumpFalse));
      resolve_labels.push_back(out.size());
      emit_le(out, out.end(), static_cast<std::uint16_t>(instr.data));
      out.resize(out.size() + 2);
      break;
    case Instruction::JumpTrue:
      out.push_back(static_cast<std::uint8_t>(OpCode::JumpTrue));
      resolve_labels.push_back(out.size());
      emit_le(out, out.end(), static_cast<std::uint16_t>(instr.data));
      out.resize(out.size() + 2);
      break;
    case Instruction::Call:
      out.push_back(static_cast<std::uint8_t>(OpCode::Call));
      emit_le(out, out.end(), static_cast<std::uint16_t>(instr.data));
      break;
    case Instruction::Return:
      if (instr.data == 0)
        out.push_back(static_cast<std::uint8_t>(OpCode::LoadNull));
      out.push_back(static_cast<std::uint8_t>(OpCode::Return));
      break;
    case Instruction::Dispatch: {
      out.push_back(static_cast<std::uint8_t>(OpCode::Dispatch));
      emit_le(out, out.end(), static_cast<std::uint16_t>(instr.data));
      auto argc_ext = *(++it);
      emit_le(out, out.end(), static_cast<std::uint16_t>(argc_ext.data));
    } break;
    case Instruction::Construct:
      out.push_back(static_cast<std::uint8_t>(OpCode::Construct));
      break;
    case Instruction::StoreVField: {
      out.push_back(static_cast<std::uint8_t>(OpCode::StoreVField));
      emit_le(out, out.end(), static_cast<std::uint16_t>(instr.data));
      auto field_ext = *(++it);
      emit_le(out, out.end(), static_cast<std::uint16_t>(field_ext.data));
    } break;
    case Instruction::Extension:
      assert(false && "Extensions are pulled earlier, can't reach");
    }
  }

  write_le(
    out, body_size, static_cast<std::uint64_t>(out.size() - body_start));
}

// <table_size:8><sub_count:2><subs...:?>
void BytecodeEmitter::emit_subs(
  ProgramIr const &prog, std::vector<std::uint8_t> &out) {
  auto head_start = out.size();
  out.resize(out.size() + sizeof(std::size_t) + sizeof(std::uint16_t));
  auto head_end = out.size();
  auto table_size = head_start;
  auto sub_count = table_size + sizeof(std::size_t);

  for (auto const &sub : prog.funcs) {
    emit_le(out, out.end(), static_cast<std::uint16_t>(sub.id));
    emit_body(sub.body, out);
  }

  write_le(
    out, table_size, static_cast<std::uint64_t>(out.size() - head_start - 8));
  write_le(out, sub_count, static_cast<std::uint16_t>(prog.funcs.size()));
}

void BytecodeEmitter::emit_classes(
  ProgramIr const &prog, std::vector<std::uint8_t> &out) {
  auto head_start = out.size();
  out.resize(out.size() + sizeof(std::size_t) + sizeof(std::uint16_t));
  auto head_end = out.size();
  auto table_size = head_start;
  auto class_count = table_size + sizeof(std::size_t);

  for (auto const table : prog.classes) {
    emit_le(out, out.end(), static_cast<std::uint16_t>(table.id));
    emit_le(out, out.end(), static_cast<std::size_t>(table.fields.size()));
    for (auto const field : table.fields) {
      emit_le(out, out.end(), static_cast<std::uint8_t>(field.is_public));
      emit_le(out, out.end(), static_cast<std::uint8_t>(field.is_static));
      emit_le(out, out.end(), static_cast<std::uint16_t>(field.name));
    }
    emit_le(out, out.end(), static_cast<std::size_t>(table.methods.size()));
    for (auto const meth : table.methods) {
      emit_le(out, out.end(), static_cast<std::uint8_t>(meth.is_public));
      emit_le(out, out.end(), static_cast<std::uint8_t>(meth.is_static));
      emit_le(out, out.end(), static_cast<std::uint16_t>(meth.name));
      emit_le(out, out.end(), static_cast<std::uint16_t>(meth.id));
    }
  }

  write_le(
    out, table_size, static_cast<std::uint64_t>(out.size() - head_start - 8));
  write_le(out, class_count, static_cast<std::uint16_t>(prog.classes.size()));
}

void BytecodeEmitter::resolve_ids(
  ProgramIr const &prog, std::vector<std::uint8_t> &bytes) {
  for (auto const &res : resolve_labels) {
    std::uint16_t id = (bytes[res + 1] << 8) | bytes[res];
    auto off = static_cast<std::int32_t>(
      static_cast<std::ptrdiff_t>(labels[id]) -
      static_cast<std::ptrdiff_t>(global_offset + res) + 1);
    write_le(bytes, res, static_cast<std::int32_t>(off));
  }

  resolve_labels.clear();
}
