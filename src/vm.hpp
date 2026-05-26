#pragma once
#include "irgen.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

struct PeaObject;

class Vm;

struct PeaNaN {
  struct Array {
    std::vector<PeaNaN> data;
    std::vector<std::size_t> dims;
  };

  std::uint64_t bits;

  // TODO: Handle true QNaN
  static PeaNaN of_double(double dbl);
  static PeaNaN of_char(char c);
  static PeaNaN of_func(std::size_t sz);
  static PeaNaN of_null();
  static PeaNaN of_ref(PeaNaN *ref, bool is_local);
  static PeaNaN of_obj(PeaObject *obj);
  static PeaNaN of_class(std::uint16_t c);

  bool is_num() const;
  bool is_null() const;
  bool is_chr() const;
  bool is_fn() const;
  bool is_ref() const;
  bool is_obj() const;
  bool is_class() const;

  double num() const;
  char chr() const;
  std::size_t fn() const;
  PeaNaN *ref() const;
  bool ref_local() const;
  PeaObject *obj() const;
  std::uint16_t cls() const;

  PeaNaN get_member(Vm &vm, std::uint16_t id) const;
  std::optional<std::size_t> get_method(Vm &vm, std::uint16_t id) const;

  std::uint16_t what() const;

  void deref();
  PeaNaN const &canon() const;
  PeaNaN &canon();

  std::optional<double> coerce_num(Vm &vm);
  std::optional<std::string> coerce_str(Vm &vm);
  bool is_truthy(Vm &vm);

  bool equals(Vm &vm, PeaNaN other);
};

enum class InternalObj : std::uint16_t {
  String = (1 << 16) - 1,
  Array = (1 << 16) - 2,
  Char = (1 << 16) - 3,
  Number = (1 << 16) - 4,
  Function = (1 << 16) - 5,
  Null = (1 << 16) - 6,
  Class = (1 << 16) - 7,
};

struct PeaObject {
  std::uint16_t kind;
};

struct PeaObjString {
  PeaObject obj;
  std::size_t len;
  char str[];

  static PeaObjString *make(std::size_t len, char const *data);
};

struct PeaObjArray {
  PeaObject obj;
  std::size_t dim;
  std::size_t *dims;
  std::size_t len;
  PeaNaN elems[];

  static PeaObjArray *make(std::size_t dim, std::size_t *dims, std::size_t len);
};

struct PeaObjGeneric {
  PeaObject obj;
  PeaNaN vals[];
};

struct CallFrame {
  std::size_t return_to;
  std::size_t stack_at;
  std::size_t shadows_at;
  std::uint16_t in_class;
};

struct PeaVTable {
  struct Method {
    bool is_public;
    std::uint16_t name;
    std::size_t sub;

    explicit Method(std::uint16_t name, std::size_t sub)
        : is_public(true), name(name), sub(sub) {};
    explicit Method(bool is_public, std::uint16_t name, std::size_t sub)
        : is_public(is_public), name(name), sub(sub) {};
  };

  struct Field {
    bool is_public;
    std::uint16_t name;
    PeaNaN val;
  };

  std::vector<Method> methods, static_methods;
  std::vector<Field> fields, static_fields;
};

class Vm;

struct BuiltinFns {
  enum Ids : std::size_t {
    ARRAY_AT = (1ULL << 48) - 1,
    PRINTLN = (1ULL << 48) - 2,
    ARRAY_TOSTRING = (1ULL << 48) - 3,
    NUM_TOSTRING = (1ULL << 48) - 4,
    NULL_TOSTRING = (1ULL << 48) - 5,
    CHAR_TOSTRING = (1ULL << 48) - 6,
    STRING_TOSTRING = (1ULL << 48) - 7,
    FUNCTION_TOSTRING = (1ULL << 48) - 8,
    NUM_TONUM = (1ULL << 48) - 9,
    CHAR_TONUM = (1ULL << 48) - 10,
    NULL_TONUM = (1ULL << 48) - 11,
    STRING_TONUM = (1ULL << 48) - 12,
    ARRAY_ISTRUTHY = (1ULL << 48) - 13,
    NUM_ISTRUTHY = (1ULL << 48) - 14,
    NULL_ISTRUTHY = (1ULL << 48) - 15,
    CHAR_ISTRUTHY = (1ULL << 48) - 16,
    STRING_ISTRUTHY = (1ULL << 48) - 17,
    FUNCTION_ISTRUTHY = (1ULL << 48) - 18,
    ARRAY_EQUALS = (1ULL << 48) - 19,
    NUM_EQUALS = (1ULL << 48) - 20,
    NULL_EQUALS = (1ULL << 48) - 21,
    CHAR_EQUALS = (1ULL << 48) - 22,
    STRING_EQUALS = (1ULL << 48) - 23,
    FUNCTION_EQUALS = (1ULL << 48) - 24,
  };

  static PeaNaN array_at(Vm &vm, std::uint16_t argc);
  static PeaNaN println(Vm &vm, std::uint16_t argc);
  static PeaNaN array_tostring(Vm &vm, std::uint16_t argc);
  static PeaNaN num_tostring(Vm &vm, std::uint16_t argc);
  static PeaNaN null_tostring(Vm &vm, std::uint16_t argc);
  static PeaNaN char_tostring(Vm &vm, std::uint16_t argc);
  static PeaNaN string_tostring(Vm &vm, std::uint16_t argc);
  static PeaNaN function_tostring(Vm &vm, std::uint16_t argc);
  static PeaNaN num_tonum(Vm &vm, std::uint16_t argc);
  static PeaNaN char_tonum(Vm &vm, std::uint16_t argc);
  static PeaNaN null_tonum(Vm &vm, std::uint16_t argc);
  static PeaNaN string_tonum(Vm &vm, std::uint16_t argc);
  static PeaNaN array_istruthy(Vm &vm, std::uint16_t argc);
  static PeaNaN num_istruthy(Vm &vm, std::uint16_t argc);
  static PeaNaN null_istruthy(Vm &vm, std::uint16_t argc);
  static PeaNaN char_istruthy(Vm &vm, std::uint16_t argc);
  static PeaNaN string_istruthy(Vm &vm, std::uint16_t argc);
  static PeaNaN function_istruthy(Vm &vm, std::uint16_t argc);
  static PeaNaN array_equals(Vm &vm, std::uint16_t argc);
  static PeaNaN num_equals(Vm &vm, std::uint16_t argc);
  static PeaNaN null_equals(Vm &vm, std::uint16_t argc);
  static PeaNaN char_equals(Vm &vm, std::uint16_t argc);
  static PeaNaN string_equals(Vm &vm, std::uint16_t argc);
  static PeaNaN function_equals(Vm &vm, std::uint16_t argc);

  // helper:
  static std::string to_string(Vm &vm, PeaNaN val) ;
};

class Vm {
public:
  explicit Vm(std::vector<std::uint8_t> bytes);

  void run(std::optional<std::size_t> until = {});

private:
  friend class BuiltinFns;
  friend class PeaNaN;

  void move_start();
  void error(std::string_view const str);
  template <typename Int> Int read();
  template <typename Int> Int read_at(std::size_t pos);

  std::vector<std::uint8_t> bytes{};
  std::vector<PeaNaN> stack{};
  std::vector<PeaNaN> variables{};
  std::vector<CallFrame> call_stack{};
  std::vector<std::pair<std::size_t, PeaNaN>> shadow_stack{};
  std::array<PeaVTable, (1ULL << 16)> vtables{};
  std::vector<std::function<PeaNaN(Vm &, std::uint16_t)>> internal_fns{
    BuiltinFns::array_at,
    BuiltinFns::println,
    BuiltinFns::array_tostring,
    BuiltinFns::num_tostring,
    BuiltinFns::null_tostring,
    BuiltinFns::char_tostring,
    BuiltinFns::string_tostring,
    BuiltinFns::function_tostring,
    BuiltinFns::num_tonum,
    BuiltinFns::char_tonum,
    BuiltinFns::null_tonum,
    BuiltinFns::string_tonum,
    BuiltinFns::array_istruthy,
    BuiltinFns::num_istruthy,
    BuiltinFns::null_istruthy,
    BuiltinFns::char_istruthy,
    BuiltinFns::string_istruthy,
    BuiltinFns::function_istruthy,
    BuiltinFns::array_equals,
    BuiltinFns::num_equals,
    BuiltinFns::null_equals,
    BuiltinFns::char_equals,
    BuiltinFns::string_equals,
    BuiltinFns::function_equals,
  };

  void var_set(std::size_t id, PeaNaN val);
  void var_def(std::size_t id);
  PeaNaN var_get(std::size_t id);
  PeaNaN var_ref(std::size_t id);

  void dispatch_call(PeaNaN callee, std::uint16_t argc, std::uint16_t in_class = static_cast<std::uint16_t>(InternalObj::Null));
  PeaNaN dispatch_util(std::size_t callee, std::uint16_t argc, std::uint16_t in_class = static_cast<std::uint16_t>(InternalObj::Null));
  std::size_t util_on{0};

  std::size_t ip{};
};
