#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
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
  size_t ip{};
};
