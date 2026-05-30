#include "irgen.hpp"
#include "lexer.hpp"
#include "vm.hpp"

#include <cassert>
#include <cstring>
#include <print>
#include <string>
#include <system_error>

PeaNaN BuiltinFns::array_at(Vm &vm, std::uint16_t argc) {
  auto self = vm.stack[vm.stack.size() - argc];
  self.deref();
  auto dimc = argc - 1;
  auto arr = reinterpret_cast<PeaObjArray *>(self.obj());
  if (dimc != arr->dim) {
    vm.error("Indexing array of wrong dimensions");
    return PeaNaN::of_null();
  }
  std::size_t step = 1;
  std::size_t ind = 0;
  for (std::size_t i = 0; i < dimc; ++i) {
    auto n = vm.stack[vm.stack.size() - i - 1].coerce_num(vm);
    if (!n) {
      vm.error("Trying to index array with non-number");
      return PeaNaN::of_null();
    }
    if (*n < 0 || *n > arr->dims[dimc - 1 - i]) {
      vm.error("Indexing out of bounds");
      return PeaNaN::of_null();
    }
    ind += step * (*n - 1);
    step *= arr->dims[dimc - 1 - i];
  }
  return PeaNaN::of_ref(&arr->elems[ind], false);
}

void array_to_string_helper(Vm &vm,
  const PeaObjArray *arr,
  std::size_t current_dim,
  std::size_t &flat_index,
  std::string &out) {
  // Base case: We reached the leaf level, print the elements of the last
  // dimension
  if (current_dim == arr->dim - 1) {
    out += "[";
    for (std::size_t i = 0; i < arr->dims[current_dim]; ++i) {
      // Convert the individual PeaNaN element to string (calling your main
      // function)
      out += BuiltinFns::to_string(vm, arr->elems[flat_index++]);
      if (i < arr->dims[current_dim] - 1) {
        out += ", ";
      }
    }
    out += "]";
    return;
  }

  // Step case: Recursively wrap inner dimensions
  out += "[";
  for (std::size_t i = 0; i < arr->dims[current_dim]; ++i) {
    array_to_string_helper(vm, arr, current_dim + 1, flat_index, out);
    if (i < arr->dims[current_dim] - 1) {
      out += ", ";
    }
  }
  out += "]";
}

std::string BuiltinFns::to_string(Vm &vm, PeaNaN val) {
  if (val.is_num()) {
    return std::format("{:g}", val.num());
  } else if (val.is_chr()) {
    return std::string(1, val.chr());
  } else if (val.is_null()) {
    return "null";
  } else if (val.is_fn()) {
    return "<function>";
  } else if (val.is_obj()) {
    switch (static_cast<InternalObj>(val.obj()->kind)) {
    case InternalObj::String:
      return std::string(reinterpret_cast<char *>(
        reinterpret_cast<PeaObjString *>(val.obj())->str));
      break;
    case InternalObj::Array: {
      auto arr = reinterpret_cast<PeaObjArray *>(val.obj());
      // Edge case: Empty array or 0 dimensions
      if (arr->dim == 0 || arr->len == 0) {
        return "[]";
      }

      std::string result;
      std::size_t flat_index = 0;
      array_to_string_helper(vm, arr, 0, flat_index, result);
      return result;
    } break;
    default: {
      auto meth = val.get_method(vm, PEA_ID_TOSTRING);
      if (!meth)
        return "<object>";
      vm.stack.push_back(val);
      auto res = vm.dispatch_util(*meth, 1, val.what());
      if (!res.is_obj() ||
          res.obj()->kind != static_cast<std::uint16_t>(InternalObj::String))
        return "<object>";
      auto str = reinterpret_cast<PeaObjString *>(res.obj());
      return std::string{ str->str, str->len };
    } break;
    }
  }
  return "";
}

PeaNaN BuiltinFns::println(Vm &vm, std::uint16_t argc) {
  for (std::uint16_t i = 1; i < argc; ++i) {
    auto val = vm.stack[vm.stack.size() - argc + i];
    val.deref();
    std::print("{}", to_string(vm, val));
  }
  std::println("");
  return PeaNaN::of_null();
}

PeaNaN BuiltinFns::array_tostring(Vm &vm, std::uint16_t argc) {
  auto o = vm.stack.back();
  o.deref();
  auto str = to_string(vm, o);
  auto s = PeaObjString::make(str.size(), str.c_str());
  return PeaNaN::of_obj(reinterpret_cast<PeaObject *>(s));
}

PeaNaN BuiltinFns::num_tostring(Vm &vm, std::uint16_t argc) {
  auto o = vm.stack.back();
  o.deref();
  auto str = to_string(vm, o);
  auto s = PeaObjString::make(str.size(), str.c_str());
  return PeaNaN::of_obj(reinterpret_cast<PeaObject *>(s));
}

PeaNaN BuiltinFns::null_tostring(Vm &vm, std::uint16_t argc) {
  auto o = vm.stack.back();
  o.deref();
  auto str = to_string(vm, o);
  auto s = PeaObjString::make(str.size(), str.c_str());
  return PeaNaN::of_obj(reinterpret_cast<PeaObject *>(s));
}

PeaNaN BuiltinFns::char_tostring(Vm &vm, std::uint16_t argc) {
  auto o = vm.stack.back();
  o.deref();
  auto str = to_string(vm, o);
  auto s = PeaObjString::make(str.size(), str.c_str());
  return PeaNaN::of_obj(reinterpret_cast<PeaObject *>(s));
}

PeaNaN BuiltinFns::string_tostring(Vm &vm, std::uint16_t argc) {
  auto o = vm.stack.back();
  o.deref();
  auto str = to_string(vm, o);
  auto s = PeaObjString::make(str.size(), str.c_str());
  return PeaNaN::of_obj(reinterpret_cast<PeaObject *>(s));
}

PeaNaN BuiltinFns::function_tostring(Vm &vm, std::uint16_t argc) {
  auto o = vm.stack.back();
  o.deref();
  auto str = to_string(vm, o);
  auto s = PeaObjString::make(str.size(), str.c_str());
  return PeaNaN::of_obj(reinterpret_cast<PeaObject *>(s));
}

PeaNaN BuiltinFns::num_tonum(Vm &vm, std::uint16_t argc) {
  vm.stack.back().deref();
  return vm.stack.back();
}

PeaNaN BuiltinFns::char_tonum(Vm &vm, std::uint16_t argc) {
  vm.stack.back().deref();
  return PeaNaN::of_double(static_cast<double>(vm.stack.back().chr()));
}

PeaNaN BuiltinFns::null_tonum(Vm &vm, std::uint16_t argc) {
  vm.stack.back().deref();
  return PeaNaN::of_double(0);
}

PeaNaN BuiltinFns::string_tonum(Vm &vm, std::uint16_t argc) {
  vm.stack.back().deref();
  auto str = reinterpret_cast<PeaObjString *>(vm.stack.back().obj());
  double res;
  auto [_, ec] = std::from_chars(str->str, str->str + str->len, res);
  if (ec == std::errc()) {
    return PeaNaN::of_double(res);
  } else {
    return PeaNaN::of_null();
  }
}

PeaNaN BuiltinFns::array_istruthy(Vm &vm, std::uint16_t argc) {
  return PeaNaN::of_double(1);
}

PeaNaN BuiltinFns::num_istruthy(Vm &vm, std::uint16_t argc) {
  vm.stack.back().deref();
  return PeaNaN::of_double(vm.stack.back().num() != 0);
}

PeaNaN BuiltinFns::null_istruthy(Vm &vm, std::uint16_t argc) {
  return PeaNaN::of_double(0);
}

PeaNaN BuiltinFns::char_istruthy(Vm &vm, std::uint16_t argc) {
  vm.stack.back().deref();
  return PeaNaN::of_double(vm.stack.back().chr() != '\0');
}

PeaNaN BuiltinFns::string_istruthy(Vm &vm, std::uint16_t argc) {
  vm.stack.back().deref();
  auto str = reinterpret_cast<PeaObjString *>(vm.stack.back().obj());
  return PeaNaN::of_double(str->len != 0);
}

PeaNaN BuiltinFns::function_istruthy(Vm &vm, std::uint16_t argc) {
  return PeaNaN::of_double(1);
}

PeaNaN BuiltinFns::array_equals(Vm &vm, std::uint16_t argc) {
  vm.stack[vm.stack.size() - argc].deref();
  vm.stack[vm.stack.size() - argc + 1].deref();
  auto self =
    reinterpret_cast<PeaObjArray *>(vm.stack[vm.stack.size() - argc].obj());
  auto other =
    reinterpret_cast<PeaObjArray *>(vm.stack[vm.stack.size() - argc + 1].obj());

  if (self->dim != other->dim)
    return PeaNaN::of_double(0);

  for (std::size_t i = 0; i < self->dim; ++i)
    if (self->dims[i] != other->dims[i])
      return PeaNaN::of_double(0);

  for (std::size_t i = 0; i < self->len; ++i)
    if (!self->elems[i].equals(vm, other->elems[i]))
      return PeaNaN::of_double(0);

  return PeaNaN::of_double(1);
}

PeaNaN BuiltinFns::num_equals(Vm &vm, std::uint16_t argc) {
  vm.stack[vm.stack.size() - argc].deref();
  vm.stack[vm.stack.size() - argc + 1].deref();
  auto self = vm.stack[vm.stack.size() - argc].num();
  auto other = vm.stack[vm.stack.size() - argc + 1].num();
  return PeaNaN::of_double(self == other);
}

PeaNaN BuiltinFns::null_equals(Vm &vm, std::uint16_t argc) {
  return PeaNaN::of_double(1);
}

PeaNaN BuiltinFns::char_equals(Vm &vm, std::uint16_t argc) {
  vm.stack[vm.stack.size() - argc].deref();
  vm.stack[vm.stack.size() - argc + 1].deref();
  auto self = vm.stack[vm.stack.size() - argc].chr();
  auto other = vm.stack[vm.stack.size() - argc + 1].chr();
  return PeaNaN::of_double(self == other);
}

PeaNaN BuiltinFns::string_equals(Vm &vm, std::uint16_t argc) {
  vm.stack[vm.stack.size() - argc].deref();
  vm.stack[vm.stack.size() - argc + 1].deref();
  auto self =
    reinterpret_cast<PeaObjString *>(vm.stack[vm.stack.size() - argc].obj());
  auto other = reinterpret_cast<PeaObjString *>(
    vm.stack[vm.stack.size() - argc + 1].obj());

  return PeaNaN::of_double(std::strcmp(self->str, other->str) == 0);
}

PeaNaN BuiltinFns::function_equals(Vm &vm, std::uint16_t argc) {
  vm.stack[vm.stack.size() - argc].deref();
  vm.stack[vm.stack.size() - argc + 1].deref();
  auto self = vm.stack[vm.stack.size() - argc].fn();
  auto other = vm.stack[vm.stack.size() - argc + 1].fn();
  return PeaNaN::of_double(self == other);
}

PeaNaN BuiltinFns::array_length(Vm &vm, std::uint16_t argc) {
  auto arr = reinterpret_cast<PeaObjArray *>(
    vm.stack[vm.stack.size() - argc].canon().obj());
  auto dim = vm.stack[vm.stack.size() - argc + 1].coerce_num(vm);

  if (!dim) {
    vm.error("Array.length expected number");
    return PeaNaN::of_null();
  }

  return PeaNaN::of_double(
    arr->dim < *dim ? 0 : arr->dims[static_cast<std::size_t>(*dim) - 1]);
}

PeaNaN BuiltinFns::array_dim(Vm &vm, std::uint16_t argc) {
  auto arr = reinterpret_cast<PeaObjArray *>(
    vm.stack[vm.stack.size() - argc].canon().obj());

  return PeaNaN::of_double(arr->dim);
}
