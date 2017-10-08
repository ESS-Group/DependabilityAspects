#include <iostream>
#include <stdexcept>

void func (int v) { std::cout << "in func " << v << std::endl; }
void (*func_ptr)(int) = &func;

int main () {
  (*func_ptr)(0); // call ok
  
  func_ptr = (void (*)(int))815; // corrupt function pointer
  (*func_ptr)(1); // call invalid
}
