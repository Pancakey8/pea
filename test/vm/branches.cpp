#include "test_runner.hpp"

void test_branches() {
  {
    Vm vm = vm_run("if 1 then\n"
                   "let x = 3\n"
                   "else\n"
                   "let x = 5\n"
                   "end if\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 3.0);
  }
  {
    Vm vm = vm_run("if 0 then\n"
                   "let x = 3\n"
                   "else\n"
                   "let x = 5\n"
                   "end if\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 5.0);
  }
  {
    Vm vm = vm_run("if 0 then\n"
                   "let x = 3\n"
                   "else if 1 then\n"
                   "let x = 5\n"
                   "else\n"
                   "let x = 7\n"
                   "end if\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 5.0);
  }
  {
    Vm vm = vm_run("dim x = 0\n"
                   "do while i < 10\n"
                   "let x = x + i\n"
                   "let i = i + 1\n"
                   "loop\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 45.0);
  }
  {
    Vm vm = vm_run("dim x = 0\n"
                   "do while 1\n"
                   "if i = 5 then\n"
                   "let i = 6\n"
                   "continue\n"
                   "end if\n"
                   "let x = x + i\n"
                   "let i = i + 1\n"
                   "if i >= 10 then\n"
                   "break\n"
                   "end if\n"
                   "loop\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 40.0);
  }
  {
    Vm vm = vm_run("dim x = 0\n"
                   "do\n"
                   "let x = 10\n"
                   "loop while i <> 0\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 10.0);
  }
  {
    Vm vm = vm_run("dim x = 0\n"
                   "for i = 0 to 10 step 2\n"
                   "let x = x + i\n"
                   "end for\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 30.0);
  }
  {
    Vm vm = vm_run("dim x = 0\n"
                   "dim i = 2\n"
                   "for i = 0 to 10 step i\n"
                   "let x = x + i\n"
                   "end for\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 30.0);
  }
  {
    Vm vm = vm_run("dim x = 1\n"
                   "dim i = 1\n"
                   "lp:\n"
                   "if i < 6 then\n"
                   "let x = x * i\n"
                   "let i = i + 1\n"
                   "goto lp\n"
                   "end if\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 120.0);
  }
  {
    Vm vm = vm_run("dim x\n"
                   "sub add(a, b)\n"
                   "  return a + b\n"
                   "end sub\n"
                   "let x = add(2, 3)\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 5.0);
  }
  {
    Vm vm = vm_run("dim x\n"
                   "sub add(a, b)\n"
                   "  dim y = a + b\n"
                   "  return y\n"
                   "end sub\n"
                   "let x = y + add(2, 3)\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 5.0);
  }
  {
    Vm vm = vm_run("dim x\n"
                   "sub add(a, b)\n"
                   "  let y = a + b\n"
                   "  return y\n"
                   "end sub\n"
                   "let x = add(2, 3) + y\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 10.0);
  }
  {
    Vm vm = vm_run("dim x\n"
                   "sub fibo(n)\n"
                   "if n < 2 then\n"
                   "return n\n"
                   "else\n"
                   "return fibo(n - 1) + fibo(n - 2)\n"
                   "end if\n"
                   "end sub\n"
                   "let x = fibo(10)\n");
    expect(vm.variables[0].is_num() && vm.variables[0].num() == 55.0);
  }
}
