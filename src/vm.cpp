#include "vm.hpp"
#include "bytecode.hpp"
#include <bit>
#include <cstdlib>
#include <iostream>
#include <print>

constexpr std::uint64_t QNAN = 0x7ff8000000000000ULL;
constexpr std::uint64_t TAG_SHIFT = 48;
constexpr std::uint64_t TAG_MASK = 0x0007000000000000ULL;
constexpr std::uint64_t PAYLOAD_MASK = 0x0000FFFFFFFFFFFFULL;

enum Tag : std::uint64_t {
  TAG_NULL = 1,
  TAG_CHAR = 2,
  TAG_STR = 3,
  TAG_FN = 4
};

constexpr std::uint64_t make_tag(Tag t) {
  return static_cast<std::uint64_t>(t) << TAG_SHIFT;
}

constexpr bool is_boxed(std::uint64_t b) { return (b & QNAN) == QNAN; }

PeaNaN PeaNaN::of_double(double dbl) {
  return { std::bit_cast<std::uint64_t>(dbl) };
}

PeaNaN PeaNaN::of_char(char c) {
  return { QNAN | make_tag(TAG_CHAR) | static_cast<std::uint8_t>(c) };
}

PeaNaN PeaNaN::of_string(std::string *s) {
  return { QNAN | make_tag(TAG_STR) |
           (reinterpret_cast<std::uint64_t>(s) & PAYLOAD_MASK) };
}

PeaNaN PeaNaN::of_func(std::size_t sz) {
  return { QNAN | make_tag(TAG_FN) |
           (static_cast<std::uint64_t>(sz) & PAYLOAD_MASK) };
}

PeaNaN PeaNaN::of_null() { return { QNAN | make_tag(TAG_NULL) }; }

bool PeaNaN::is_num() const { return !is_boxed(bits); }

bool PeaNaN::is_chr() const {
  return is_boxed(bits) && ((bits & TAG_MASK) == make_tag(TAG_CHAR));
}

bool PeaNaN::is_str() const {
  return is_boxed(bits) && ((bits & TAG_MASK) == make_tag(TAG_STR));
}

bool PeaNaN::is_null() const {
  return is_boxed(bits) && ((bits & TAG_MASK) == make_tag(TAG_NULL));
}

bool PeaNaN::is_fn() const {
  return is_boxed(bits) && ((bits & TAG_MASK) == make_tag(TAG_FN));
}

std::size_t PeaNaN::fn() const {
  return static_cast<std::size_t>(bits & PAYLOAD_MASK);
}

std::string *PeaNaN::str() const {
  std::uint64_t raw = bits & PAYLOAD_MASK;
  if (raw & (1ULL << 47)) {
    raw |= 0xFFFF000000000000ULL;
  }
  return reinterpret_cast<std::string *>(raw);
}

char PeaNaN::chr() const { return static_cast<char>(bits & 0xFF); }

double PeaNaN::num() const { return std::bit_cast<double>(bits); }

Vm::Vm(std::vector<std::uint8_t> bytes)
    : bytes(std::move(bytes)), variables{ 1ULL << 16, PeaNaN::of_null() } {
  move_start();
}

void Vm::run() {
  while (ip < bytes.size()) {
    auto instr = ip;
    auto op = read<std::uint8_t>();
    switch (static_cast<OpCode>(op)) {
    case OpCode::Add: {
      std::cout << "Add:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Sub: {
      std::cout << "Sub:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Mul: {
      std::cout << "Mul:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Div: {
      std::cout << "Div:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::IDiv: {
      std::cout << "IDiv:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Pow: {
      std::cout << "Pow:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Mod: {
      std::cout << "Mod:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::And: {
      std::cout << "And:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Or: {
      std::cout << "Or:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Eq: {
      std::cout << "Eq:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Neq: {
      std::cout << "Neq:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Lt: {
      std::cout << "Lt:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Gt: {
      std::cout << "Gt:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Lte: {
      std::cout << "Lte:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Gte: {
      std::cout << "Gte:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Concat: {
      std::cout << "Concat:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Pos: {
      std::cout << "Pos:\n";
      auto op = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Neg: {
      std::cout << "Neg:\n";
      auto op = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::Not: {
      std::cout << "Not:\n";
      auto op = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_null()); // TODO
    } break;
    case OpCode::LoadVar: {
      std::cout << "LoadVar:\n";
      auto var = read<std::uint16_t>();
      stack.push_back(variables[var]);
    } break;
    case OpCode::DefineVar: {
      std::cout << "DefineVar:\n";
      auto var = read<std::uint16_t>();
      auto dim = read<std::uint16_t>();
      for (std::uint16_t i = 0; i < dim; ++i)
        stack.pop_back(); // TODO
    } break;
    case OpCode::DefineVarT: {
      std::cout << "DefineVarT:\n";
      auto var = read<std::uint16_t>();
      auto dim = read<std::uint16_t>();
      auto type = read<std::uint16_t>(); // TODO
      for (std::uint16_t i = 0; i < dim; ++i)
        stack.pop_back(); // TODO
    } break;
    case OpCode::StoreVar: {
      std::cout << "StoreVar:\n";
      auto var = read<std::uint16_t>();
      variables[var] = stack.back();
      stack.pop_back();
    } break;
    case OpCode::LoadConst: {
      std::cout << "LoadConst:\n";
      auto off = read<std::size_t>();
      auto kind = read_at<std::uint8_t>(off);
      switch (kind) {
      case 0: { // Number
        auto bits = read_at<std::uint64_t>(off + 1);
        auto dbl = std::bit_cast<double>(bits);
        stack.push_back(PeaNaN::of_double(dbl));
      } break;
      case 1: { // Char
        auto bits = read_at<std::uint8_t>(off + 1);
        stack.push_back(PeaNaN::of_char(static_cast<char>(bits)));
      } break;
      case 2: { // String
        auto len = read_at<std::uint64_t>(off + 1);
        std::string *s =
          new std::string{}; // TODO: Garbage collect, handle double pointer
        s->resize(len);
        std::copy(
          bytes.begin() + off + 9, bytes.begin() + off + 9 + len, s->begin());
        stack.push_back(PeaNaN::of_string(s));
      } break;
      case 3: { // FuncPtr
        auto ptr = read_at<std::size_t>(off + 1);
        stack.push_back(PeaNaN::of_func(ptr));
      } break;
      default:
        return error("Invalid constant type");
      }
    } break;
    case OpCode::LoadNull: {
      std::cout << "LoadNull:\n";
      stack.push_back(PeaNaN::of_null());
    } break;
    case OpCode::Pop: {
      std::cout << "Pop:\n";
      stack.pop_back();
    } break;
    case OpCode::Goto: {
      std::cout << "Goto:\n";
      auto off = read<std::int32_t>();
      ip = instr + off;
    } break;
    case OpCode::JumpFalse: {
      std::cout << "JumpFalse:\n";
      auto off = read<std::int32_t>();
      auto val = stack.back();
      stack.pop_back();
      if (val.is_num() && val.num() == 0)
        ip = instr + off;
    } break;
    case OpCode::JumpTrue: {
      std::cout << "JumpTrue:\n";
      auto off = read<std::int32_t>();
      auto val = stack.back();
      stack.pop_back();
      if (val.is_num() && val.num() != 0)
        ip = instr + off;
    } break;
    case OpCode::Call: {
      std::cout << "Call:\n";
      auto argc = read<std::uint16_t>();
      for (std::uint16_t i = 0; i < argc; ++i)
        stack.pop_back(); // TODO
      stack.push_back(PeaNaN::of_null());
    } break;
    case OpCode::Return: {
      std::cout << "Return:\n";
      // TODO: Won't hit yet
    } break;
    default:
      return error("Invalid instruction");
    }
  }
}

void Vm::move_start() {
  if (bytes[ip++] != 'P' || bytes[ip++] != 'E' || bytes[ip++] != 'A')
    return error("Invalid fileformat");
  auto consts = read<std::size_t>();
  ip += consts;
  auto subs = read<std::size_t>();
  ip += subs;
  auto body = read<std::size_t>();
}

void Vm::error(std::string_view const str) {
  std::println("PeaVM: Error: {}", str);
  std::abort();
}

template <typename Int> Int Vm::read() {
  std::array<std::uint8_t, sizeof(Int)> b{};
  std::copy(bytes.begin() + ip, bytes.begin() + ip + sizeof(Int), b.begin());
  if constexpr (std::endian::native == std::endian::big) {
    std::reverse(b.begin(), b.end());
  }
  Int n = std::bit_cast<Int>(b);
  ip += sizeof(Int);
  return n;
}

template <typename Int> Int Vm::read_at(std::size_t pos) {
  std::array<std::uint8_t, sizeof(Int)> b{};
  std::copy(bytes.begin() + pos, bytes.begin() + pos + sizeof(Int), b.begin());
  if constexpr (std::endian::native == std::endian::big) {
    std::reverse(b.begin(), b.end());
  }
  return std::bit_cast<Int>(b);
}
