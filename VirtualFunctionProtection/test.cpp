#include <string.h>
#include <iostream>
using namespace std;


class A {
public:
  int a;
public:
  virtual void foo() const { cout << "A::foo()" << endl; }
  A() { cout << "ctor of A" << endl; }
};

class B : public A {
public:
  void foo() const { cout << "B::foo()" << endl; }
  B() { cout << "ctor of B" << endl; }
};


int main() {
  B b;
  b.a = 4;
  b.foo();
  
  memset(&b, 0x0, sizeof(void*)); // corrupt vptr with zeros
  ((A*)&b)->foo(); // not-resolved virtual function call (b.foo() will be resolved)
  
  memset(&b, 0xFF, sizeof(void*)); // corrupt vptr with ones
  ((A*)&b)->foo(); // dito
  
  return 0;
}
