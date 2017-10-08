#include <string.h>
#include <stdio.h>

void foo() {
  // corrupt frame pointer and return address
  memset(__builtin_frame_address(0), 0xAA, sizeof(void*) * 2);
}

int main() {
  void (* volatile foo_ptr)() = foo;
  foo_ptr();
  printf("successfully returned from foo()\n");
}
