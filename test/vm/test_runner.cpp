#include "test_runner.hpp"
#include "bytecode.hpp"

#include "operations.cpp"

Vm vm_run(std::string const &input) {
  Lexer lexer{ input };
  Parser parser{ lexer };
  auto program = parser.parse();
  if (!program) {
    std::println("Error {}:{} to {}:{}: {}",
      program.error().range.start.line,
      program.error().range.start.col,
      program.error().range.end.line,
      program.error().range.end.col,
      program.error().message);
  }
  IrGen gen{};
  auto ir = gen.generate(*program);
  if (!ir) {
    std::println("Error {}:{} to {}:{}: {}",
      ir.error().range.start.line,
      ir.error().range.start.col,
      ir.error().range.end.line,
      ir.error().range.end.col,
      ir.error().message);
  }
  std::vector<std::uint8_t> bytecode{};
  BytecodeEmitter emitter{ *ir };
  emitter.emit(bytecode);
  Vm vm{ bytecode };
  vm.run();
  return vm;
}

std::size_t pass{}, total{};

int main() {
  test_operations();
  std::println("{} / {} tests passed", pass, total);
}
