#include <iostream>

int factorial(int n);

int main() {
  for(int i=0; i<100; i++) {
    std::cout << factorial(i) << " " << std::flush;
  }
  return 0;
}

int factorial(int n)
{
  return (n == 1 || n == 0) ? 1 : factorial(n - 1) * n;
}
