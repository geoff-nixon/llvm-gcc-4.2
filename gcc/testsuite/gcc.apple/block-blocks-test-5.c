/* APPLE LOCAL file 5932809 */
/* { dg-options "-fblocks" } */
/* { dg-do run } */

#include <stdio.h>
void * _NSConcreteStackBlock;
void _Block_byref_assign_copy(void * dst, void *src){}
void _Block_byref_release(void*src){}
extern void exit(int);

typedef double (^myblock)(int);


double test(myblock I) {
  return I(42);
}

int main() {
  int x = 1;
  __byref int y = 2;
  __byref br_x;
  int y1;
  double res = test(^(int z){|y| y = x+z; return (double)x; }); /* { dg-warning "has been deprecated in blocks" } */
  printf("result = %f  x = %d y = %d\n", res, x, y);
  if (x != 1 || y != 43)
   exit(1);

  br_x = x;
  y1 = y;

  res = test(^(int z){|br_x| br_x = br_x+z; return (double)y1; }); /* { dg-warning "has been deprecated in blocks" } */
  printf("result = %f  br_x = %d y1 = %d\n", res, br_x, y1);
  if (br_x != 43 || y1 != 43)
    exit(1);

  return 0;
}

