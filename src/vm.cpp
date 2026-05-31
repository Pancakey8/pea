#include "vm.hpp"
#include "bytecode.hpp"
#include "irgen.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <print>
#include <ranges>

constexpr std::uint64_t QNAN = 0x7ff8000000000000ULL;
constexpr std::uint64_t TAG_SHIFT = 48;
constexpr std::uint64_t TAG_MASK = 0x0007000000000000ULL;
constexpr std::uint64_t PAYLOAD_MASK = 0x0000FFFFFFFFFFFFULL;

enum Tag : std::uint64_t {
  TAG_NULL = 1,
  TAG_CHAR = 2,
  TAG_FN = 3,
  TAG_REF = 4,
  TAG_OBJ = 5,
  TAG_CLASS = 6
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

PeaNaN PeaNaN::of_func(std::size_t sz) {
  return { QNAN | make_tag(TAG_FN) |
           (static_cast<std::uint64_t>(sz) & PAYLOAD_MASK) };
}

PeaNaN PeaNaN::of_null() { return { QNAN | make_tag(TAG_NULL) }; }

PeaNaN PeaNaN::of_ref(PeaNaN *ref, bool is_local) {
  return { QNAN | make_tag(TAG_REF) |
           (reinterpret_cast<std::uint64_t>(ref) & PAYLOAD_MASK) | is_local };
}

PeaNaN PeaNaN::of_obj(PeaObject *ref) {
  return { QNAN | make_tag(TAG_OBJ) |
           (reinterpret_cast<std::uint64_t>(ref) & PAYLOAD_MASK) };
}

PeaNaN PeaNaN::of_class(std::uint16_t c) {
  return { QNAN | make_tag(TAG_CLASS) | c };
}

bool PeaNaN::is_num() const { return !is_boxed(bits); }

bool PeaNaN::is_chr() const {
  return is_boxed(bits) && ((bits & TAG_MASK) == make_tag(TAG_CHAR));
}

bool PeaNaN::is_null() const {
  return is_boxed(bits) && ((bits & TAG_MASK) == make_tag(TAG_NULL));
}

bool PeaNaN::is_fn() const {
  return is_boxed(bits) && ((bits & TAG_MASK) == make_tag(TAG_FN));
}

bool PeaNaN::is_ref() const {
  return is_boxed(bits) && ((bits & TAG_MASK) == make_tag(TAG_REF));
}

bool PeaNaN::is_obj() const {
  return is_boxed(bits) && ((bits & TAG_MASK) == make_tag(TAG_OBJ));
}

bool PeaNaN::is_class() const {
  return is_boxed(bits) && ((bits & TAG_MASK) == make_tag(TAG_CLASS));
}

std::size_t PeaNaN::fn() const {
  return static_cast<std::size_t>(bits & PAYLOAD_MASK);
}

PeaObject *PeaNaN::obj() const {
  std::uint64_t raw = bits & PAYLOAD_MASK;
  if (raw & (1ULL << 47)) {
    raw |= 0xFFFF000000000000ULL;
  }
  return reinterpret_cast<PeaObject *>(raw);
}

char PeaNaN::chr() const { return static_cast<char>(bits & 0xFF); }

std::uint16_t PeaNaN::cls() const {
  return static_cast<std::uint16_t>(bits & 0xFFFF);
}

double PeaNaN::num() const { return std::bit_cast<double>(bits); }

PeaNaN *PeaNaN::ref() const {
  std::uint64_t raw = bits & PAYLOAD_MASK;
  if (raw & (1ULL << 47)) {
    raw |= 0xFFFF000000000000ULL;
  }
  return reinterpret_cast<PeaNaN *>(raw & ~1);
}

bool PeaNaN::ref_local() const {
  std::uint64_t raw = bits & PAYLOAD_MASK;
  return raw & 1;
}

PeaNaN PeaNaN::get_member(Vm &vm, std::uint16_t id) const {
  auto self = *this;
  self.deref();
  bool see_privates{ !vm.call_stack.empty() &&
                     vm.call_stack.back().in_class == self.what() };

  if (self.is_class()) {
    auto &stats = vm.vtables[self.cls()].static_fields;
    for (std::size_t i = 0; i < stats.size(); ++i) {
      if ((see_privates || stats[i].is_public) && stats[i].name == id)
        return PeaNaN::of_ref(&stats[i].val, false);
    }
    return PeaNaN::of_null();
  }

  if (!self.is_obj())
    return PeaNaN::of_null();
  if (self.what() == static_cast<std::uint16_t>(InternalObj::Array) ||
      self.what() == static_cast<std::uint16_t>(InternalObj::String))
    return PeaNaN::of_null();

  auto &fields = vm.vtables[self.what()].fields;
  for (std::size_t i = 0; i < fields.size(); ++i) {
    if ((see_privates || fields[i].is_public) && fields[i].name == id)
      return PeaNaN::of_ref(
        &reinterpret_cast<PeaObjGeneric *>(self.obj())->vals[i], false);
  }

  return PeaNaN::of_null();
}

std::optional<std::size_t> PeaNaN::get_method(Vm &vm, std::uint16_t id) const {
  auto self = *this;
  self.deref();
  bool see_privates{ !vm.call_stack.empty() &&
                     vm.call_stack.back().in_class == self.what() };

  if (self.is_class()) {
    auto table = vm.vtables[self.cls()];
    if (auto it = std::find_if(table.static_methods.begin(),
          table.static_methods.end(),
          [see_privates, id](
            auto x) { return (see_privates || x.is_public) && x.name == id; });
      it != table.static_methods.end()) {
      return it->sub;
    }
  }

  PeaVTable table = vm.vtables[self.what()];

  if (auto it = std::find_if(table.methods.begin(),
        table.methods.end(),
        [see_privates, id](
          auto x) { return (see_privates || x.is_public) && x.name == id; });
    it != table.methods.end()) {
    return it->sub;
  }

  if (id == PEA_ID_COPY)
    return BuiltinFns::ANY_COPY;

  return {};
}

std::uint16_t PeaNaN::what() const {
  auto self = *this;
  self.deref();
  if (self.is_num())
    return static_cast<std::uint16_t>(InternalObj::Number);
  else if (self.is_null())
    return static_cast<std::uint16_t>(InternalObj::Null);
  else if (self.is_chr())
    return static_cast<std::uint16_t>(InternalObj::Char);
  else if (self.is_fn())
    return static_cast<std::uint16_t>(InternalObj::Function);
  else if (self.is_obj())
    return self.obj()->kind;
  else if (self.is_class())
    return self.cls();

  assert(false && "Unreachable");
  return static_cast<std::uint16_t>(InternalObj::Null);
}

void PeaNaN::deref() {
  if (is_ref()) {
    *this = *this->ref();
  }
}

PeaNaN const &PeaNaN::canon() const {
  if (is_ref()) {
    return *this->ref();
  } else {
    return *this;
  }
}

PeaNaN &PeaNaN::canon() {
  if (is_ref()) {
    return *this->ref();
  } else {
    return *this;
  }
}

std::optional<double> PeaNaN::coerce_num(Vm &vm) {
  auto self = *this;
  self.deref();

  if (self.is_num())
    return self.num();

  if (auto meth = self.get_method(vm, PEA_ID_TONUM)) {
    vm.stack.push_back(self);
    auto val = vm.dispatch_util(*meth, 1, self.what());
    if (!val.is_num())
      return {};
    return val.num();
  } else {
    return {};
  }
}

std::optional<std::string> PeaNaN::coerce_str(Vm &vm) {
  auto self = *this;
  self.deref();

  PeaObjString *str;
  if (self.is_obj() &&
      self.obj()->kind == static_cast<std::uint16_t>(InternalObj::String)) {
    str = reinterpret_cast<PeaObjString *>(self.obj());
  } else {
    auto meth = self.get_method(vm, PEA_ID_TOSTRING);
    if (!meth)
      return {};
    vm.stack.push_back(self);
    auto val = vm.dispatch_util(*meth, 1, self.what());
    if (!val.is_obj() ||
        val.obj()->kind != static_cast<std::uint16_t>(InternalObj::String))
      return {};
    str = reinterpret_cast<PeaObjString *>(val.obj());
  }

  std::string s{ str->str, str->len };
  return s;
}

bool PeaNaN::is_truthy(Vm &vm) {
  auto self = *this;
  self.deref();
  if (auto meth = self.get_method(vm, PEA_ID_ISTRUTHY)) {
    vm.stack.push_back(self);
    auto val = vm.dispatch_util(*meth, 1, self.what());
    if (!val.is_num())
      return false;
    return val.num() != 0;
  } else {
    return false;
  }
}

bool PeaNaN::equals(Vm &vm, PeaNaN other) {
  auto self = *this;
  self.deref();
  other.deref();
  if (self.is_class() && other.is_class())
    return self.cls() == other.cls();
  if (self.what() != other.what())
    return false;

  if (auto meth = self.get_method(vm, PEA_ID_EQUALS)) {
    vm.stack.push_back(self);
    vm.stack.push_back(other);
    auto val = vm.dispatch_util(*meth, 2, self.what());
    return val.is_truthy(vm);
  } else {
    return false;
  }
}

PeaNaN PeaNaN::copy(Vm &vm) const {
  auto self = *this;
  self.deref();
  if (!self.is_obj())
    return self;
  auto obj = self.obj();

  switch (obj->kind) {
  case static_cast<std::uint16_t>(InternalObj::Array): {
    auto arr = reinterpret_cast<PeaObjArray *>(obj);

    auto dims =
      static_cast<std::size_t *>(calloc(arr->dim, sizeof(std::size_t)));
    std::copy(arr->dims, arr->dims + arr->dim, dims);

    auto cpy = PeaObjArray::make(arr->dim, dims, arr->len);
    for (std::size_t i = 0; i < cpy->len; ++i)
      cpy->elems[i] = arr->elems[i].copy(vm);

    return PeaNaN::of_obj(reinterpret_cast<PeaObject *>(cpy));
  } break;
  case static_cast<std::uint16_t>(InternalObj::String): {
    auto str = reinterpret_cast<PeaObjString *>(obj);
    auto cpy = PeaObjString::make(str->len, str->str);
    return PeaNaN::of_obj(reinterpret_cast<PeaObject *>(cpy));
  } break;
  default: {
    auto gen = reinterpret_cast<PeaObjGeneric *>(self.obj());
    auto const &tbl = vm.vtables[gen->obj.kind];

    auto cpy = static_cast<PeaObjGeneric *>(malloc(
						   offsetof(PeaObjGeneric, vals) + tbl.fields.size() * sizeof(PeaNaN)));

    cpy->obj.kind = gen->obj.kind;
    for (std::size_t i = 0; i < tbl.fields.size(); ++i)
      cpy->vals[i] = gen->vals[i].copy(vm);

    return PeaNaN::of_obj(reinterpret_cast<PeaObject *>(cpy));
  } break;
  }
}

PeaObjString *PeaObjString::make(std::size_t len, char const *data) {
  auto mem = std::malloc(offsetof(PeaObjString, str) + len + 1);
  PeaObjString *obj = ::new (mem) PeaObjString;
  obj->obj =
    PeaObject{ .kind = static_cast<std::uint16_t>(InternalObj::String) };
  obj->len = len;
  std::memcpy(obj->str, data, len);
  obj->str[obj->len] = '\0';
  return obj;
}

PeaObjArray *PeaObjArray::make(
  std::size_t dim, std::size_t *dims, std::size_t len) {
  auto mem = std::malloc(offsetof(PeaObjArray, elems) + len * sizeof(PeaNaN));
  PeaObjArray *arr = ::new (mem) PeaObjArray;
  arr->obj =
    PeaObject{ .kind = static_cast<std::uint16_t>(InternalObj::Array) };
  arr->dim = dim;
  arr->dims = dims;
  arr->len = len;
  return arr;
}

Vm::Vm(std::vector<std::uint8_t> bytes)
    : bytes(std::move(bytes)), variables{ 1ULL << 16, PeaNaN::of_null() } {
  vtables[static_cast<std::uint16_t>(InternalObj::String)] = {
    {
      PeaVTable::Method(PEA_ID_TOSTRING, BuiltinFns::STRING_TOSTRING),
      PeaVTable::Method(PEA_ID_TONUM, BuiltinFns::STRING_TONUM),
      PeaVTable::Method(PEA_ID_ISTRUTHY, BuiltinFns::STRING_ISTRUTHY),
      PeaVTable::Method(PEA_ID_EQUALS, BuiltinFns::STRING_EQUALS),
    },
    {}
  };
  vtables[static_cast<std::uint16_t>(InternalObj::Array)] = {
    {
      PeaVTable::Method(PEA_ID_AT, BuiltinFns::ARRAY_AT),
      PeaVTable::Method(PEA_ID_TOSTRING, BuiltinFns::ARRAY_TOSTRING),
      PeaVTable::Method(PEA_ID_ISTRUTHY, BuiltinFns::ARRAY_ISTRUTHY),
      PeaVTable::Method(PEA_ID_EQUALS, BuiltinFns::ARRAY_EQUALS),
      PeaVTable::Method(PEA_ID_LENGTH, BuiltinFns::ARRAY_LENGTH),
      PeaVTable::Method(PEA_ID_DIM, BuiltinFns::ARRAY_DIM),
    },
    {},
    {},
    {}
  };
  vtables[static_cast<std::uint16_t>(InternalObj::Char)] = {
    {
      PeaVTable::Method(PEA_ID_TOSTRING, BuiltinFns::CHAR_TOSTRING),
      PeaVTable::Method(PEA_ID_TONUM, BuiltinFns::CHAR_TONUM),
      PeaVTable::Method(PEA_ID_ISTRUTHY, BuiltinFns::CHAR_ISTRUTHY),
      PeaVTable::Method(PEA_ID_EQUALS, BuiltinFns::CHAR_EQUALS),
    },
    {},
    {},
    {}
  };
  vtables[static_cast<std::uint16_t>(InternalObj::Number)] = {
    {
      PeaVTable::Method(PEA_ID_TOSTRING, BuiltinFns::NUM_TOSTRING),
      PeaVTable::Method(PEA_ID_TONUM, BuiltinFns::NUM_TONUM),
      PeaVTable::Method(PEA_ID_ISTRUTHY, BuiltinFns::NUM_ISTRUTHY),
      PeaVTable::Method(PEA_ID_EQUALS, BuiltinFns::NUM_EQUALS),
    },
    {},
    {},
    {}
  };
  vtables[static_cast<std::uint16_t>(InternalObj::Function)] = {
    {
      PeaVTable::Method(PEA_ID_TOSTRING, BuiltinFns::FUNCTION_TOSTRING),
      PeaVTable::Method(PEA_ID_ISTRUTHY, BuiltinFns::FUNCTION_ISTRUTHY),
      PeaVTable::Method(PEA_ID_EQUALS, BuiltinFns::FUNCTION_EQUALS),
    },
    {},
    {},
    {}
  };
  vtables[static_cast<std::uint16_t>(InternalObj::Null)] = {
    {
      PeaVTable::Method(PEA_ID_TOSTRING, BuiltinFns::NULL_TOSTRING),
      PeaVTable::Method(PEA_ID_TONUM, BuiltinFns::NULL_TONUM),
      PeaVTable::Method(PEA_ID_ISTRUTHY, BuiltinFns::NULL_ISTRUTHY),
      PeaVTable::Method(PEA_ID_EQUALS, BuiltinFns::NULL_EQUALS),
    },
    {},
    {},
    {}
  };
  variables[PEA_ID_PRINTLN] = PeaNaN::of_func(BuiltinFns::PRINTLN);
  signal_error = [](Error error) {
    std::println("VM Exception {}:{} to {}:{}: {}",
      error.range.start.line,
      error.range.start.col,
      error.range.end.line,
      error.range.end.col,
      error.message);
    std::abort();
  };
}

void Vm::run(std::optional<std::size_t> until) {
  auto max = until ? *until : bytes.size();
  while (ip < max) {
    auto instr = ip;
    auto op = read<std::uint8_t>();
    switch (static_cast<OpCode>(op)) {
    case OpCode::Add: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num(*this);
      auto opl = left.coerce_num(*this);
      if (!opr || !opl)
        return error("'+' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl + *opr));
    } break;
    case OpCode::Sub: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num(*this);
      auto opl = left.coerce_num(*this);
      if (!opr || !opl)
        return error("'-' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl - *opr));
    } break;
    case OpCode::Mul: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num(*this);
      auto opl = left.coerce_num(*this);
      if (!opr || !opl)
        return error("'*' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl * *opr));
    } break;
    case OpCode::Div: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num(*this);
      auto opl = left.coerce_num(*this);
      if (!opr || !opl)
        return error("'/' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl / *opr));
    } break;
    case OpCode::IDiv: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num(*this);
      auto opl = left.coerce_num(*this);
      if (!opr || !opl)
        return error("'\\' requires number, number");
      stack.push_back(PeaNaN::of_double(std::trunc(*opl / *opr)));
    } break;
    case OpCode::Pow: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num(*this);
      auto opl = left.coerce_num(*this);
      if (!opr || !opl)
        return error("'^' requires number, number");
      stack.push_back(PeaNaN::of_double(std::pow(*opl, *opr)));
    } break;
    case OpCode::Mod: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num(*this);
      auto opl = left.coerce_num(*this);
      if (!opr || !opl)
        return error("'mod' requires number, number");
      stack.push_back(PeaNaN::of_double(std::fmod(*opl, *opr)));
    } break;
    case OpCode::And: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.is_truthy(*this);
      auto opl = left.is_truthy(*this);
      stack.push_back(PeaNaN::of_double(opl && opr));
    } break;
    case OpCode::Or: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.is_truthy(*this);
      auto opl = left.is_truthy(*this);
      stack.push_back(PeaNaN::of_double(opl || opr));
    } break;
    case OpCode::Eq: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_double(left.equals(*this, right)));
    } break;
    case OpCode::Neq: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      stack.push_back(PeaNaN::of_double(!left.equals(*this, right)));
    } break;
    case OpCode::Lt: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num(*this);
      auto opl = left.coerce_num(*this);
      if (!opr || !opl)
        return error("'<' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl < *opr));
    } break;
    case OpCode::Gt: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num(*this);
      auto opl = left.coerce_num(*this);
      if (!opr || !opl)
        return error("'>' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl > *opr));
    } break;
    case OpCode::Lte: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num(*this);
      auto opl = left.coerce_num(*this);
      if (!opr || !opl)
        return error("'<=' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl <= *opr));
    } break;
    case OpCode::Gte: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto opr = right.coerce_num(*this);
      auto opl = left.coerce_num(*this);
      if (!opr || !opl)
        return error("'>=' requires number, number");
      stack.push_back(PeaNaN::of_double(*opl >= *opr));
    } break;
    case OpCode::Concat: {
      auto right = stack.back();
      stack.pop_back();
      auto left = stack.back();
      stack.pop_back();
      auto rstr = right.coerce_str(*this);
      auto lstr = left.coerce_str(*this);
      if (!lstr || !rstr)
        return error("'&' requires string, string");
      auto cat = *lstr + *rstr;
      auto obj = PeaObjString::make(cat.size(), cat.c_str());
      stack.push_back(PeaNaN::of_obj(reinterpret_cast<PeaObject *>(obj)));
    } break;
    case OpCode::Pos: {
      auto op = stack.back();
      stack.pop_back();
      auto n = op.coerce_num(*this);
      if (!n)
        return error("'+' requires number");
      stack.push_back(PeaNaN::of_double(*n));
    } break;
    case OpCode::Neg: {
      auto op = stack.back();
      stack.pop_back();
      auto n = op.coerce_num(*this);
      if (!n)
        return error("'-' requires number");
      stack.push_back(PeaNaN::of_double(-*n));
    } break;
    case OpCode::Not: {
      auto op = stack.back();
      stack.pop_back();
      auto b = op.is_truthy(*this);
      stack.push_back(PeaNaN::of_double(!b));
    } break;
    case OpCode::Deref: {
      stack.back().deref();
    } break;
    case OpCode::Member: {
      auto field = read<std::uint16_t>();
      auto obj = stack.back();
      stack.pop_back();
      stack.push_back(obj.get_member(*this, field));
    } break;
    case OpCode::Dispatch: {
      auto meth = read<std::uint16_t>();
      auto argc = read<std::uint16_t>();
      auto self = stack[stack.size() - argc];

      if (auto meth_off = self.get_method(*this, meth)) {
        dispatch_call(PeaNaN::of_func(*meth_off), argc, self.what());
      } else {
        return error("Dispatching non-existent method");
      }
    } break;
    case OpCode::LoadVar: {
      auto var = read<std::uint16_t>();
      auto v = var_ref(var);
      stack.push_back(v);
    } break;
    case OpCode::DefineVar: {
      auto var = read<std::uint16_t>();
      auto dim = read<std::uint16_t>();
      var_def(var);
      if (dim > 0) {
        std::size_t total = 1;
        auto dims =
          static_cast<std::size_t *>(std::calloc(dim, sizeof(std::size_t)));
        for (std::uint16_t i = 0; i < dim; ++i) {
          auto num = stack.back().coerce_num(*this);
          if (!num)
            return error("Array dimension must be a number");
          if (num <= 0)
            return error("Array dimension must be > 0");
          dims[dim - i - 1] = *num;
          total *= *num;
          stack.pop_back();
        }
        auto arr = PeaObjArray::make(dim, dims, total);
        for (std::size_t i = 0; i < total; ++i)
          arr->elems[i] = PeaNaN::of_null();

        var_set(var, PeaNaN::of_obj(reinterpret_cast<PeaObject *>(arr)));
      }
    } break;
    case OpCode::StoreVar: {
      auto lhs = stack.back();
      stack.pop_back();
      auto val = stack.back();
      stack.pop_back();
      if (!lhs.is_ref())
        return error("Assignment to non-reference is forbidden");
      *lhs.ref() = val;
    } break;
    case OpCode::LoadConst: {
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
        auto str =
          PeaObjString::make(len, reinterpret_cast<char *>(&bytes[off + 9]));
        stack.push_back(PeaNaN::of_obj(reinterpret_cast<PeaObject *>(str)));
      } break;
      case 3: { // FuncPtr
        auto ptr = read_at<std::size_t>(off + 1);
        stack.push_back(PeaNaN::of_func(ptr));
      } break;
      case 4: {
        auto ptr = read_at<std::size_t>(off + 1);
        stack.push_back(PeaNaN::of_class(ptr));
      } break;
      default:
        return error("Invalid constant type");
      }
    } break;
    case OpCode::LoadNull: {
      stack.push_back(PeaNaN::of_null());
    } break;
    case OpCode::Pop: {
      auto val = stack.back();
      stack.pop_back();
    } break;
    case OpCode::Goto: {
      auto off = read<std::int32_t>();
      ip = instr + off;
    } break;
    case OpCode::JumpFalse: {
      auto off = read<std::int32_t>();
      auto val = stack.back();
      stack.pop_back();
      if (!val.is_truthy(*this))
        ip = instr + off;
    } break;
    case OpCode::JumpTrue: {
      auto off = read<std::int32_t>();
      auto val = stack.back();
      stack.pop_back();
      if (val.is_truthy(*this))
        ip = instr + off;
    } break;
    case OpCode::Call: {
      auto argc = read<std::uint16_t>();
      auto callee = stack[stack.size() - argc - 1];
      if (callee.canon().is_obj()) {
        auto meth_at = callee.get_method(*this, PEA_ID_AT);
        if (!meth_at)
          return error("Indexing non-indexable object");
        dispatch_call(
          PeaNaN::of_func(*meth_at), argc + 1, callee.canon().what());
      } else if (callee.canon().is_fn()) {
        dispatch_call(callee, argc + 1);
      }
    } break;
    case OpCode::Return: {
      auto ret = stack.back();
      stack.pop_back();
      if (ret.is_ref() && ret.ref_local())
        ret.deref();
      auto frame = call_stack.back();
      call_stack.pop_back();
      stack.resize(frame.stack_at);
      shadow_stack.resize(frame.shadows_at);
      stack.push_back(ret);
      ip = frame.return_to;
    } break;
    case OpCode::Construct: {
      auto classptr = stack.back();
      stack.pop_back();
      classptr.deref();

      if (!classptr.is_class())
        return error("Cannot construct from non-class");

      auto obj = static_cast<PeaObjGeneric *>(
        malloc(offsetof(PeaObjGeneric, vals) +
               vtables[classptr.cls()].fields.size() * sizeof(PeaNaN)));

      obj->obj.kind = classptr.cls();
      std::size_t idx = 0;
      for (auto const f : vtables[classptr.cls()].fields) {
        obj->vals[idx++] = f.val.copy(*this);
      }

      stack.push_back(PeaNaN::of_obj(reinterpret_cast<PeaObject *>(obj)));
    } break;
    case OpCode::StoreVField: {
      auto table = read<std::uint16_t>();
      auto field = read<std::uint16_t>();
      auto val = stack.back();
      stack.pop_back();
      if (auto fld = std::find_if(vtables[table].fields.begin(),
            vtables[table].fields.end(),
            [&field](auto x) { return x.name == field; });
        fld != vtables[table].fields.end()) {
        fld->val = val;
      } else if (auto sfld = std::find_if(vtables[table].static_fields.begin(),
                   vtables[table].static_fields.end(),
                   [&field](auto x) { return x.name == field; });
        sfld != vtables[table].static_fields.end()) {
        sfld->val = val;
      } else
        return error("Field not found");
    } break;
    case OpCode::ProgPrefix: {
      auto off = read<std::size_t>();
      ip += off;
    } break;
    default:
      return error("Invalid instruction");
    }
  }
}

void Vm::boot() {
  if (bytes[ip] == static_cast<std::uint8_t>(OpCode::ProgPrefix)) {
    ip += 9;
  }
  if (bytes[ip++] != 'P' || bytes[ip++] != 'E' || bytes[ip++] != 'A')
    return error("Invalid fileformat");
  auto consts = read<std::size_t>();
  ip += consts;
  auto subs = read<std::size_t>();
  ip += subs;
  auto classes = read<std::size_t>();
  auto class_count = read<std::uint16_t>();
  for (std::uint16_t i = 0; i < class_count; ++i) {
    PeaVTable table{};
    auto id = read<std::uint16_t>();
    auto fields = read<std::size_t>();
    for (std::size_t j = 0; j < fields; ++j) {
      bool is_public = read<std::uint8_t>();
      bool is_static = read<std::uint8_t>();
      auto name = read<std::uint16_t>();
      if (is_static) {
        table.static_fields.push_back(
          PeaVTable::Field{ is_public, name, PeaNaN::of_null() });
      } else {
        table.fields.push_back(
          PeaVTable::Field{ is_public, name, PeaNaN::of_null() });
      }
    }
    auto methods = read<std::size_t>();
    for (std::size_t j = 0; j < methods; ++j) {
      bool is_public = read<std::uint8_t>();
      bool is_static = read<std::uint8_t>();
      auto name = read<std::uint16_t>();
      auto sub = read<std::uint64_t>();
      if (is_static) {
        table.static_methods.push_back(
          PeaVTable::Method{ is_public, name, sub });
      } else {
        table.methods.push_back(PeaVTable::Method{ is_public, name, sub });
      }
    }
    vtables[id] = std::move(table);
  }
  auto body = read<std::size_t>();
}

void Vm::error(std::string_view const str) {
  ip = bytes.size();
  signal_error(Error{ std::string{ str }, {} });
}

void Vm::on_error(std::function<void(Error)> on_error) {
  signal_error = on_error;
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
        return PeaNaN::of_ref(&shadow_stack[i].second, true);
      }
    }
  }

  return variables[id].is_ref() ? variables[id]
                                : PeaNaN::of_ref(&variables[id], false);
}

void Vm::dispatch_call(
  PeaNaN callee, std::uint16_t argc, std::uint16_t in_class) {
  callee.deref();
  if (callee.is_fn()) {
    if (callee.fn() > bytes.size()) {
      std::uint16_t off = (1ULL << 48) - 1 - callee.fn();
      auto top = stack.size() - argc;
      auto val = internal_fns[off](*this, argc);
      stack.resize(top);
      stack.push_back(val);
    } else {
      std::size_t top = stack.size() - argc;
      for (std::size_t i = top; i < stack.size(); ++i)
        if (stack[i].is_ref() && stack[i].ref_local())
          stack[i] = PeaNaN::of_ref(stack[i].ref(), false);
      call_stack.push_back({ ip, top, shadow_stack.size(), in_class });
      ip = callee.fn() + 8;
    }
  } else {
    return error("Trying to call non-function");
  }
}

PeaNaN Vm::dispatch_util(
  std::size_t callee, std::uint16_t argc, std::uint16_t in_class) {
  auto top = stack.size() - argc;
  auto until = ip;
  dispatch_call(PeaNaN::of_func(callee), argc, in_class);
  run(until);
  auto val = stack.back();
  stack.resize(top);
  val.deref();
  return val;
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

void Vm::append(std::vector<std::uint8_t> bytes) {
  this->bytes.insert(this->bytes.end(), bytes.begin(), bytes.end());
}
