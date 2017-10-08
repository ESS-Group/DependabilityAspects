#include <string.h>
#include <iostream>
using namespace std;

class Rectangle {
protected:
  long int width;
  long int height;

public:
  Rectangle() { width = 0; height = 0; }

  long int getArea() const { return width*height; }
  void setWidth(long int width) { this->width = width; }
  void setHeight(long int height) { this->height = height; }

  virtual void print() { cout << "Rectangle: " << width << " x " << height << endl; }

  static void injectFault(Rectangle& r, long int fault) { memcpy(&r.height, &fault, sizeof(fault)); }
};

class Square : public Rectangle {
protected:
  void setHeight() { height = width; }

public:
  Square() { width = 1; setHeight(); }
  Square(long int radius) { width = radius; setHeight(); }

  virtual void print() { cout << "Square: " << width << " x " << height << endl; }

  static void injectFault(Square& s, long int fault) { memcpy(&s.width, &fault, sizeof(fault)); }
};

class Circle {
private:
  int radius;
  static int instances;

public:
  static Circle single;
  static char name[6];

  Circle() : radius(0) { ++instances; }
  ~Circle() { --instances; }

  void print() { cout << name << ": r = " << radius << " (instances = " << instances << ")" << endl; }
  static void print_single() { single.print(); }

  static void injectFault(Circle& c, int fault) { memcpy(&c.radius, &fault, sizeof(fault)); }
  static void injectFault(int fault) { memcpy(&instances, &fault, sizeof(fault)); }
};

int Circle::instances = 0;
Circle Circle::single;
char Circle::name[] = {'C', 'i', 'r', 'l', 'e', '\0'};


int main() {
  Rectangle r;
  r.setWidth(2);
  r.setHeight(3);
  Rectangle::injectFault(r, 1); // height: 3 -> 1
  r.print();

  Rectangle r2(r); // test copy constructor
  Rectangle::injectFault(r2, 5); // height: 3 -> 5
  Rectangle r3;
  r3 = r2; // test assignment operator
  r3.print();
  
  Square s(5);
  Square::injectFault(s, 4); // radius: 5 -> 4
  s.print();

  Circle::injectFault(8); // instances: 1 -> 8
  Circle::injectFault(Circle::single, 3); // radius: 0 -> 3
  memset(&Circle::name[3], 0, sizeof(Circle::name[3])); // 'l' -> 0
  Circle::print_single();

  // nullify complete object (no recovery possible; check on program exit)
  memset(&Circle::single, 0, sizeof(Circle::single));

  return 0;
}

