#pragma once
#include "irgen.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

struct PeaObject;

struct PeaNaN {
  struct Array {
    std::vector<PeaNaN> data;
    std::vector<std::size_t> dims;
  };

  std::uint64_t bits;

  // TODO: Handle true QNaN
  static PeaNaN of_double(double dbl);
  static PeaNaN of_char(char c);
  static PeaNaN of_string(std::string *s);
  static PeaNaN of_func(std::size_t sz);
  static PeaNaN of_null();
  static PeaNaN of_array(Array *arr);
  static PeaNaN of_ref(PeaNaN *ref);
  static PeaNaN of_obj(PeaObject *obj);

  bool is_num() const;
  bool is_null() const;
  bool is_chr() const;
  bool is_str() const;
  bool is_fn() const;
  bool is_arr() const;
  bool is_ref() const;
  bool is_obj() const;

  double num() const;
  char chr() const;
  std::string *str() const;
  std::size_t fn() const;
  Array *arr() const;
  PeaNaN *ref() const;
  PeaObject *obj() const;

  std::optional<double> coerce_num() const;
  std::optional<char> coerce_chr() const;
  std::optional<std::string *> coerce_str() const;

  bool is_truthy() const;

  bool operator==(PeaNaN const &other) const;
};

enum class InternalObj : std::uint16_t { String, Array };

struct PeaObjString {
  std::uint16_t kind{static_cast<std::uint16_t>(InternalObj::String)};
  std::size_t len;
  char str[];
};

struct PeaObjArray {
  std::uint16_t kind{static_cast<std::uint16_t>(InternalObj::Array)};
  std::size_t len;
  PeaNaN elems[];
};

struct PeaObject {
  std::uint16_t kind;
};

struct CallFrame {
  std::size_t return_to;
  std::size_t stack_at;
  std::size_t shadows_at;
};

class Vm {
public:
  explicit Vm(std::vector<std::uint8_t> bytes);

  void run();

private:
  void move_start();
  void error(std::string_view const str);
  template <typename Int> Int read();
  template <typename Int> Int read_at(std::size_t pos);

  std::vector<std::uint8_t> bytes{};
  std::vector<PeaNaN> stack{};
  std::vector<PeaNaN> variables{};
  std::vector<CallFrame> call_stack{};
  std::vector<std::pair<std::size_t, PeaNaN>> shadow_stack{};

  void var_set(std::size_t id, PeaNaN val);
  void var_def(std::size_t id);
  PeaNaN var_get(std::size_t id);
  PeaNaN var_ref(std::size_t id);

  PeaNaN member_get(PeaObject *val, std::uint16_t id);

  size_t ip{};
};
