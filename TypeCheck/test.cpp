namespace Foo {
int r = 0;
}

class A {
public:
  int x;
  void foo() {}

  void moo(A* ptr);
  static void foo2() {}
};

void A::moo(A* ptr) {
  ptr->x = ptr->x + 1;
}

class B {int y;};

int main() {
  Foo::r = 42; // global variable access

  A a;
  a.x = 42;
  a.foo();

  a.foo2();

  B b;

  A* ptr = (A*)&b; //(A*)(0x4211);

  A a2; //(*ptr); // copy ctor
  a2 = *ptr; // assign op

  a2.x = 3;
};

