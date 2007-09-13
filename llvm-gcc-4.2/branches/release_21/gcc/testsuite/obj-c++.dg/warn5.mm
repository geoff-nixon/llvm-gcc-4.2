/* APPLE LOCAL begin radar 4229905 */
/* Check to make sure that a c++ program compiled in objective-c++ mode
   has no trace of meta-data specific diagnosis coming out of compiling it.
   This is replicate of warn5.C.
*/
// { dg-do assemble  }
// { dg-options "-Wpointer-arith" }

double X(const double x) { return x; }
double Y() { return 1.0; }
double Z() { return 2.0; }

struct A {
  void bar() { }
  void foo() { }
};

typedef void (A::*pmf)();

static int mememe = &A::foo - &A::bar;  // { dg-warning "" } 
pmf b = &A::foo-1;      // { dg-warning "" } 

int main() {
    double y;
    y=X(Y-Z);   // { dg-warning "" } 
}
/* APPLE LOCAL end radar 4229905 */
