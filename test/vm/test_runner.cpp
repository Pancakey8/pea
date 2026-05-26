#include "test_runner.hpp"
#include "bytecode.hpp"

#include "operations.cpp"
#include "branches.cpp"
#include "classes.cpp"

std::unique_ptr<Vm> vm_run(std::string const &input) {
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
  auto vm = std::make_unique<Vm>(bytecode);
  vm->run();
  return vm;
}

std::size_t pass{}, total{};

int main() {
  test_operations();
  test_branches();
  test_classes();
  std::println("{} / {} tests passed", pass, total);
}
