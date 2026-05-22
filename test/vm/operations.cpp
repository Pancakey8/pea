#include "test_runner.hpp"

void test_operations() {
  {
    Vm vm = vm_run("let x = 2 + 3\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 5.0);
  }
  {
    Vm vm = vm_run("let x = 2.5 - 3\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == -0.5);
  }
  {
    Vm vm = vm_run("let x = 2.5 * \"3\"\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 7.5);
  }
  {
    Vm vm = vm_run("let x = 20 / 8\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 2.5);
  }
  {
    Vm vm = vm_run("let x = 20 \\ 8\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 2.0);
  }
  {
    Vm vm = vm_run("let x = ('9' - '0') ^ 2\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 81.0);
  }
  {
    Vm vm = vm_run("let x = 20\nlet y = x mod 8\n");
    expect(vm.variables[1].is_num() && vm.variables[1].num() == 4.0);
  }
  {
    Vm vm = vm_run("let x = 1 and 0\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 0.0);
  }
  {
    Vm vm = vm_run("dim x\nlet x = 0 or not_defined\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 0.0);
  }
  {
    Vm vm = vm_run("let x = \"hey\" = \"hey\"\nlet y = \"3\" = 3\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 1.0);
    expect(vm.variables[1].is_num() && vm.variables[1].num() == 1.0);
  }
  {
    Vm vm = vm_run("let x = \"hey\" <> \"hi\"\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 1.0);
  }
  {
    Vm vm = vm_run("let x = 3 < 10\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 1.0);
  }
  {
    Vm vm = vm_run("let x = 10 > 3\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 1.0);
  }
  {
    Vm vm = vm_run("let x = 3.5 <= 3.5\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 1.0);
  }
  {
    Vm vm = vm_run("let x = 8 >= 5\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 1.0);
  }
  {
    Vm vm = vm_run("let x = \"Hello \" & (2 + 3)\n");
    expect(vm.variables[0].is_str() && *vm.variables[0].str() == "Hello 5");
  }
  {
    Vm vm = vm_run("let x = +\"67\"\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 67);
  }
  {
    Vm vm = vm_run("let x = 10\nlet x = -x\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == -10);
  }
  {
    Vm vm = vm_run("dim x\nlet x = not not_defined\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 1.0);
  }
  {
    Vm vm = vm_run("dim x(2, 3, 4)\nlet x(2, 1, 3) = 5.0\n");
    expect(vm.variables[0].is_arr() &&
           vm.variables[0].arr()->data[14].is_num() &&
           vm.variables[0].arr()->data[14].num() == 5.0);
  }
}
