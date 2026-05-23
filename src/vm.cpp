#include "vm.hpp"
#include "bytecode.hpp"
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <print>
#include <string>

constexpr std::uint64_t QNAN = 0x7ff8000000000000ULL;
constexpr std::uint64_t TAG_SHIFT = 48;
constexpr std::uint64_t TAG_MASK = 0x0007000000000000ULL;
constexpr std::uint64_t PAYLOAD_MASK = 0x0000FFFFFFFFFFFFULL;

enum Tag : std::uint64_t {
  TAG_NULL = 1,
  TAG_CHAR = 2,
  TAG_STR = 3,
  TAG_FN = 4,
  TAG_ARR = 5,
  TAG_REF = 6
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

PeaNaN PeaNaN::of_array(Array *arr) {
  return { QNAN | make_tag(TAG_ARR) |
           (reinterpret_cast<std::uint64_t>(arr) & PAYLOAD_MASK) };
}

PeaNaN PeaNaN::of_ref(PeaNaN *ref) {
  return { QNAN | make_tag(TAG_REF) |
           (reinterpret_cast<std::uint64_t>(ref) & PAYLOAD_MASK) };
}

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

bool PeaNaN::is_arr() const {
  return is_boxed(bits) && ((bits & TAG_MASK) == make_tag(TAG_ARR));
}

bool PeaNaN::is_ref() const {
  return is_boxed(bits) && ((bits & TAG_MASK) == make_tag(TAG_REF));
}

std::size_t PeaNaN::fn() const {
  return static_cast<std::size_t>(bits & PAYLOAD_MASK);
}

PeaNaN::Array *PeaNaN::arr() const {
  std::uint64_t raw = bits & PAYLOAD_MASK;
  if (raw & (1ULL << 47)) {
    raw |= 0xFFFF000000000000ULL;
  }
  return reinterpret_cast<Array *>(raw);
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

PeaNaN *PeaNaN::ref() const {
  std::uint64_t raw = bits & PAYLOAD_MASK;
  if (raw & (1ULL << 47)) {
    raw |= 0xFFFF000000000000ULL;
  }
  return reinterpret_cast<PeaNaN *>(raw);
}

std::optional<double> PeaNaN::coerce_num() const {
  if (is_num()) {
    return num();
  } else if (is_chr()) {
    return static_cast<double>(chr());
  } else if (is_null()) {
    return 0;
  } else if (is_fn()) {
    return {};
  } else if (is_str()) {
    auto const s = str();
    double dbl;
    auto [_, ec] = std::from_chars(s->data(), s->data() + s->size(), dbl);
    if (ec == std::errc()) {
      return dbl;
    } else {
      return {};
    }
  } else if (is_arr()) {
    return {};
  }

  return {};
}

std::optional<char> PeaNaN::coerce_chr() const {
  if (is_num()) {
    return static_cast<char>(num());
  } else if (is_chr()) {
    return chr();
  } else if (is_null()) {
    return '\0';
  } else if (is_fn()) {
    return {};
  } else if (is_str()) {
    auto const s = str();
    return (s->size() == 1) ? std::optional{ (*s)[1] } : std::nullopt;
  } else if (is_arr()) {
    return {};
  }

  return {};
}

std::optional<std::string *> PeaNaN::coerce_str() const {
  if (is_num()) {
    return new std::string{ std::format("{:g}", num()) };
  } else if (is_chr()) {
    return new std::string(1, chr());
  } else if (is_null()) {
    return new std::string("null");
  } else if (is_fn()) {
    return {};
  } else if (is_str()) {
    return str();
  } else if (is_arr()) {
    return {};
  }

  return {};
}

bool PeaNaN::is_truthy() const {
  if (is_num()) {
    return num() != 0;
  } else if (is_chr()) {
    return chr() != '\0';
  } else if (is_null()) {
    return false;
  } else if (is_fn()) {
    return true;
  } else if (is_str()) {
    return !str()->empty();
  } else if (is_arr()) {
    return true;
  }

  return false;
}

bool PeaNaN::operator==(PeaNaN const &other) const {
  if (is_num() && other.is_num())
    return num() == other.num();

  if (is_null() && other.is_null())
    return true;

  if (is_chr() && other.is_chr())
    return chr() == other.chr();

  if (is_str() && other.is_str())
    return *str() == *other.str();

  if (is_fn() && other.is_fn())
    return fn() == other.fn();

  if (is_arr() && other.is_arr())
    return arr()->dims == other.arr()->dims && arr()->data == other.arr()->data;

  if ((is_num() && !other.is_str()) || (!is_str() && other.is_num())) {
    auto self = coerce_num();
    auto they = other.coerce_num();
    if (self && they)
      return *self == *they;
  }

  auto self = coerce_str();
  auto they = coerce_str();
  if (self && they)
    return **self == **they;

  return false;
}

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
      auto opr = right.coerce_num();
      auto opl = left.coerce_num();
      if (!opr || !opl)
        return error("'+' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl + *opr));
    } break;
    case OpCode::Sub: {
      std::cout << "Sub:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num();
      auto opl = left.coerce_num();
      if (!opr || !opl)
        return error("'-' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl - *opr));
    } break;
    case OpCode::Mul: {
      std::cout << "Mul:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num();
      auto opl = left.coerce_num();
      if (!opr || !opl)
        return error("'*' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl * *opr));
    } break;
    case OpCode::Div: {
      std::cout << "Div:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num();
      auto opl = left.coerce_num();
      if (!opr || !opl)
        return error("'/' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl / *opr));
    } break;
    case OpCode::IDiv: {
      std::cout << "IDiv:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num();
      auto opl = left.coerce_num();
      if (!opr || !opl)
        return error("'\\' requires number, number");
      stack.push_back(PeaNaN::of_double(std::trunc(*opl / *opr)));
    } break;
    case OpCode::Pow: {
      std::cout << "Pow:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num();
      auto opl = left.coerce_num();
      if (!opr || !opl)
        return error("'^' requires number, number");
      stack.push_back(PeaNaN::of_double(std::pow(*opl, *opr)));
    } break;
    case OpCode::Mod: {
      std::cout << "Mod:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num();
      auto opl = left.coerce_num();
      if (!opr || !opl)
        return error("'mod' requires number, number");
      stack.push_back(PeaNaN::of_double(std::fmod(*opl, *opr)));
    } break;
    case OpCode::And: {
      std::cout << "And:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.is_truthy();
      auto opl = left.is_truthy();
      stack.push_back(PeaNaN::of_double(opl && opr));
    } break;
    case OpCode::Or: {
      std::cout << "Or:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.is_truthy();
      auto opl = left.is_truthy();
      stack.push_back(PeaNaN::of_double(opl || opr));
    } break;
    case OpCode::Eq: {
      std::cout << "Eq:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_double(left == right));
    } break;
    case OpCode::Neq: {
      std::cout << "Neq:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_double(left != right));
    } break;
    case OpCode::Lt: {
      std::cout << "Lt:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num();
      auto opl = left.coerce_num();
      if (!opr || !opl)
        return error("'<' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl < *opr));
    } break;
    case OpCode::Gt: {
      std::cout << "Gt:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num();
      auto opl = left.coerce_num();
      if (!opr || !opl)
        return error("'>' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl > *opr));
    } break;
    case OpCode::Lte: {
      std::cout << "Lte:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num();
      auto opl = left.coerce_num();
      if (!opr || !opl)
        return error("'<=' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl <= *opr));
    } break;
    case OpCode::Gte: {
      std::cout << "Gte:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num();
      auto opl = left.coerce_num();
      if (!opr || !opl)
        return error("'>=' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl >= *opr));
    } break;
    case OpCode::Concat: {
      std::cout << "Concat:\n";
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_str();
      auto opl = left.coerce_str();
      if (!opr || !opl)
        return error("'&' requires string, string");
      auto s = new std::string{ *opl.value() + *opr.value() };
      stack.push_back(PeaNaN::of_string(s));
    } break;
    case OpCode::Pos: {
      std::cout << "Pos:\n";
      auto op = stack.back();
      stack.pop_back();
      auto n = op.coerce_num();
      if (!n)
        return error("'+' requires number");
      stack.push_back(PeaNaN::of_double(*n));
    } break;
    case OpCode::Neg: {
      std::cout << "Neg:\n";
      auto op = stack.back();
      stack.pop_back();
      auto n = op.coerce_num();
      if (!n)
        return error("'-' requires number");
      stack.push_back(PeaNaN::of_double(-*n));
    } break;
    case OpCode::Not: {
      std::cout << "Not:\n";
      auto op = stack.back();
      stack.pop_back();
      auto b = op.is_truthy();
      stack.push_back(PeaNaN::of_double(!b));
    } break;
    case OpCode::Deref: {
      auto op = stack.back();
      if (op.is_ref()) {
        stack.pop_back();
        stack.push_back(*op.ref());
      }
    } break;
    case OpCode::LoadVar: {
      std::cout << "LoadVar:\n";
      auto var = read<std::uint16_t>();
      stack.push_back(var_get(var));
    } break;
    case OpCode::LoadRef: {
      auto var = read<std::uint16_t>();
      stack.push_back(var_ref(var));
    } break;
    case OpCode::DefineVar: {
      std::cout << "DefineVar:\n";
      auto var = read<std::uint16_t>();
      auto dim = read<std::uint16_t>();
      var_def(var);
      if (dim > 0) {
        std::size_t total = 1;
        std::vector<std::size_t> dims(dim);
        for (std::uint16_t i = 0; i < dim; ++i) {
          auto num = stack.back().coerce_num();
          if (!num)
            return error("Array dimension must be a number");
          if (num < 0)
            return error("Array dimension must be > 0");
          dims[dim - i - 1] = *num;
          total *= *num;
          stack.pop_back();
        }
        std::vector<PeaNaN> nans(total, PeaNaN::of_null());
        var_set(var,
          PeaNaN::of_array(
            new PeaNaN::Array{ std::move(nans), std::move(dims) }));
      }
    } break;
    case OpCode::DefineVarT: {
      std::cout << "DefineVarT:\n";
      auto var = read<std::uint16_t>();
      auto dim = read<std::uint16_t>();
      auto type = read<std::uint16_t>(); // TODO
      var_def(var);
      if (dim > 0) {
        std::size_t total = 1;
        std::vector<std::size_t> dims(dim);
        for (std::uint16_t i = 0; i < dim; ++i) {
          auto num = stack.back().coerce_num();
          if (!num)
            return error("Array dimension must be a number");
          if (num < 0)
            return error("Array dimension must be > 0");
          dims[dim - i - 1] = *num;
          total *= *num;
          stack.pop_back();
        }
        std::vector<PeaNaN> nans(total, PeaNaN::of_null());
        var_set(var,
          PeaNaN::of_array(
            new PeaNaN::Array{ std::move(nans), std::move(dims) }));
      }
    } break;
    case OpCode::StoreVar: {
      std::cout << "StoreVar:\n";
      auto var = read<std::uint16_t>();
      var_set(var, stack.back());
      stack.pop_back();
    } break;
    case OpCode::StoreVarI: {
      auto var = read<std::uint16_t>();
      auto dimc = read<std::uint16_t>();
      auto rhs = stack.back();
      stack.pop_back();
      auto value = var_get(var);
      if (!value.is_arr())
        return error("Index assignment to non-array");
      auto arr = value.arr();
      if (dimc != arr->dims.size())
        return error("Indexing array of wrong dimensions");
      std::size_t step = 1;
      std::size_t ind = 0;
      for (std::size_t i = 0; i < dimc; ++i) {
        auto n = stack[stack.size() - i - 1].coerce_num();
        if (!n)
          return error("Trying to index array with non-number");
        if (*n < 0 || *n > arr->dims[dimc - 1 - i])
          return error("Indexing out of bounds");
        ind += step * (*n - 1);
        step *= arr->dims[dimc - 1 - i];
      }
      stack.resize(stack.size() - dimc);
      arr->data[ind] = rhs;
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
      auto val = stack.back();
      auto print_val = [&](this const auto &self, PeaNaN val) -> void {
        if (val.is_null()) {
          std::print("null");
        } else if (val.is_chr()) {
          std::print("'{}'", val.chr());
        } else if (val.is_num()) {
          std::print("{}", val.num());
        } else if (val.is_fn()) {
          std::print("{:X}", val.fn());
        } else if (val.is_str()) {
          std::print("\"{}\"", *val.str());
        } else if (val.is_arr()) {
          for (auto const &v : val.arr()->data) {
            self(v);
            std::print(",");
          }
        }
      };
      std::print("Pop ");
      print_val(val);
      std::print("\n");
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
      if (val.is_num() && val.num() == 0) // TODO
        ip = instr + off;
    } break;
    case OpCode::JumpTrue: {
      std::cout << "JumpTrue:\n";
      auto off = read<std::int32_t>();
      auto val = stack.back();
      stack.pop_back();
      if (val.is_num() && val.num() != 0) // TODO
        ip = instr + off;
    } break;
    case OpCode::Call: {
      std::cout << "Call:\n";
      auto argc = read<std::uint16_t>();
      auto callee = stack.back();
      stack.pop_back();
      if (callee.is_fn()) {
        std::println("Argc {}, stack has {}", argc, stack.size());
        std::println("Shadows {}", shadow_stack.size());
        std::size_t top = stack.size() - argc;
        call_stack.push_back({ ip, top, shadow_stack.size() });
        ip = callee.fn() + 8;
      } else if (callee.is_arr()) {
        auto arr = callee.arr();
        if (argc != arr->dims.size())
          return error("Indexing array of wrong dimensions");
        std::size_t step = 1;
        std::size_t ind = 0;
        for (std::size_t i = 0; i < argc; ++i) {
          auto n = stack[stack.size() - i - 1].coerce_num();
          if (!n)
            return error("Trying to index array with non-number");
          if (*n < 0 || *n > arr->dims[argc - 1 - i])
            return error("Indexing out of bounds");
          ind += step * (*n - 1);
          step *= arr->dims[argc - 1 - i];
        }
        stack.resize(stack.size() - argc);
        stack.push_back(arr->data[ind]);
      } else {
        return error("Trying to call non-function");
      }
    } break;
    case OpCode::Return: {
      std::cout << "Return:\n";
      auto ret = stack.back();
      stack.pop_back();
      auto frame = call_stack.back();
      call_stack.pop_back();
      std::println("Returning to {:X} with {}, have {}",
        frame.return_to,
        frame.stack_at,
        stack.size());
      stack.resize(frame.stack_at);
      shadow_stack.resize(frame.shadows_at);
      stack.push_back(ret);
      ip = frame.return_to;
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

void Vm::var_set(std::size_t id, PeaNaN val) {
  if (!call_stack.empty()) {
    auto bot = call_stack.back().shadows_at;
    for (std::size_t i = bot; i < shadow_stack.size(); ++i) {
      if (shadow_stack[i].first == id) {
        if (shadow_stack[i].second.is_ref()) {
	  *shadow_stack[i].second.ref() = val;
        } else {
          shadow_stack[i].second = val;
        }
        return;
      }
    }
  }

  if (variables[id].is_ref()) {
    *variables[id].ref() = val;
  } else {
    variables[id] = val;
  }
}

void Vm::var_def(std::size_t id) {
  if (call_stack.empty())
    return;
  auto bot = call_stack.back().shadows_at;
  for (std::size_t i = bot; i < shadow_stack.size(); ++i) {
    if (shadow_stack[i].first == id) {
      return;
    }
  }
  shadow_stack.push_back({ id, PeaNaN::of_null() });
}

PeaNaN Vm::var_get(std::size_t id) {
  if (!call_stack.empty()) {
    auto bot = call_stack.back().shadows_at;
    for (std::size_t i = bot; i < shadow_stack.size(); ++i) {
      if (shadow_stack[i].first == id) {
        if (shadow_stack[i].second.is_ref()) {
          return *shadow_stack[i].second.ref();
        } else {
          return shadow_stack[i].second;
        }
      }
    }
  }

  return variables[id].is_ref() ? *variables[id].ref() : variables[id];
}

PeaNaN Vm::var_ref(std::size_t id) {
  if (!call_stack.empty()) {
    auto bot = call_stack.back().shadows_at;
    for (std::size_t i = bot; i < shadow_stack.size(); ++i) {
      if (shadow_stack[i].first == id) {
        if (shadow_stack[i].second.is_ref())
          return shadow_stack[i].second;
        return PeaNaN::of_ref(&shadow_stack[i].second);
      }
    }
  }

  return variables[id].is_ref() ? variables[id] : PeaNaN::of_ref(&variables[id]);
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
