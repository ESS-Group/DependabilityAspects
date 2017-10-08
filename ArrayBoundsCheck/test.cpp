#include <iostream>
using namespace std;

int array[20] = { 1, 2, 3, 5, 6, 7, 8, 9, 10, 15, 20, 25, 30, 33, 44, 55, 66, 77, 88, 99 };

void test_const1() {
  cout << "Reading index 5: " << array[5] << endl;
}
void test_const2() {
  cout << "Reading index 25: " << array[25] << endl;
}
void test_const3() {
  cout << "Reading index -1: " << array[-1] << endl;
}

int multidim[5][10][15];

void multi_test1() {
  cout << "Writing index 1, 2, 9." << endl;
  multidim[1][2][9] = 0;
}

void multi_test2() {
  cout << "Writing index 1, 11, 9." << endl;
  multidim[1][11][9] = 0;
}

void loop_test() {
  for (unsigned i = 0; i <= sizeof(array)/sizeof(int); i++)
    array[i] = 0;
}

int main() {
  // ok
  multi_test1();
  test_const1();

  // not ok
  loop_test();
  multi_test2();
  test_const2();
  test_const3();
}
