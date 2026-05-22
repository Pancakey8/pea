#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

struct PeaNaN {
  std::uint64_t bits;

  // TODO: Handle true QNaN
  static PeaNaN of_double(double dbl);
  static PeaNaN of_char(char c);
  static PeaNaN of_string(std::string *s);
  static PeaNaN of_func(std::size_t sz);
  static PeaNaN of_null();

  bool is_num() const;
  bool is_null() const;
  bool is_chr() const;
  bool is_str() const;
  bool is_fn() const;

  double num() const;
  char chr() const;
  std::string *str() const;
  std::size_t fn() const;

  std::optional<double> coerce_num() const;
  std::optional<char> coerce_chr() const;
  std::optional<std::string*> coerce_str() const;

  bool is_truthy() const;
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

  size_t ip{};
};
