#include "test_runner.hpp"
#include "vm.hpp"
#include <algorithm>

void test_classes() {
  std::unique_ptr<Vm> vm;
  {
    vm = vm_run("dim res1\n"
                "dim res2\n"
                "class Foo\n"
                "dim x = 10\n"
                "public sub x()\n"
                "return this.x\n"
                "end sub\n"
                "end class\n"
                "dim f = new Foo\n"
                "let res1 = f.x()\n"
                "let res2 = f.x\n");
    expect(vm->variables[0].canon().is_num() &&
           vm->variables[0].canon().num() == 10.0);
    expect(vm->variables[1].canon().is_null());
  }
  {
    vm = vm_run("dim res1\n"
                "dim res2\n"
                "dim res3\n"
                "class Stuff\n"
                "static dim x = 5\n"
                "public static dim y\n"
                "public static sub getX()\n"
                "return Stuff.x\n"
                "end sub\n"
                "end class\n"
                "let Stuff.y = 3\n"
                "let res1 = Stuff.x\n"
                "let res2 = Stuff.y\n"
                "let res3 = Stuff.getX()\n");
    expect(vm->variables[0].canon().is_null());
    expect(vm->variables[1].canon().is_num() &&
           vm->variables[1].canon().num() == 3.0);
    expect(vm->variables[2].canon().is_num() &&
           vm->variables[2].canon().num() == 5.0);
  }
  {
    vm = vm_run("dim res(4)\n"
                "dim ctr = 1\n"
                "dim state = 10\n"
                "redecl:\n"
                "\n"
                "class foo\n"
                "dim x = state\n"
                "static dim y = 2 * state\n"
                "public sub getx()\n"
                "return this.x\n"
                "end sub\n"
                "public sub gety()\n"
                "return foo.y\n"
                "end sub\n"
                "end class\n"
                "\n"
                "let x = new foo\n"
                "let res(ctr) = x.getx()\n"
                "let ctr = ctr + 1\n"
                "let res(ctr) = x.gety()\n"
                "let ctr = ctr + 1\n"
                "\n"
                "if state = 10 then\n"
                "let state = 15\n"
                "goto redecl\n"
                "end if\n");
    expect(vm->variables[0].what() ==
           static_cast<std::uint16_t>(InternalObj::Array));
    auto arr = reinterpret_cast<PeaObjArray *>(vm->variables[0].canon().obj());
    expect(
      arr->elems[0].canon().is_num() && arr->elems[0].canon().num() == 10.0);
    expect(
      arr->elems[1].canon().is_num() && arr->elems[1].canon().num() == 20.0);
    expect(
      arr->elems[2].canon().is_num() && arr->elems[2].canon().num() == 15.0);
    expect(
      arr->elems[3].canon().is_num() && arr->elems[3].canon().num() == 30.0);
  }
  {
    vm = vm_run("dim results(2)\n"
                "dim stp = 1\n"
                "\n"
                "label:\n"
                "dim abc = 10\n"
                "\n"
                "class Foo\n"
                "  public dim x = abc * stp\n"
                "\n"
                "  public sub tostring()\n"
                "    return \"Foo(\" & this.x & \")\"\n"
                "  end sub\n"
                "end class\n"
                "\n"
                "let results(stp) = new Foo\n"
                "\n"
                "if stp = 1 then\n"
                "  let stp = 2\n"
                "  goto label\n"
                "end if\n");
    expect(vm->variables[0].what() ==
           static_cast<std::uint16_t>(InternalObj::Array));
    auto arr = reinterpret_cast<PeaObjArray *>(vm->variables[0].canon().obj());
    expect(arr->elems[0].coerce_str(*vm) == "Foo(10)");
    expect(arr->elems[1].coerce_str(*vm) == "Foo(20)");
  }
}
