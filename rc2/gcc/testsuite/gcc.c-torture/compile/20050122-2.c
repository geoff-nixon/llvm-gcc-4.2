/* APPLE LOCAL testsuite nested functions */
/* { dg-options "-fnested-functions" } */
/* LLVM LOCAL no nested functions */
/* { dg-require-fdump "" } */
/* Related to PR 19484.  */
extern void foo (void) __attribute__((noreturn));
int n;

void
g (void)
{
  __label__ lab;
  void h (void) { if (n == 2) goto lab; }
  void (*f1) (void) = foo;
  void (*f2) (void) = h;

  f2 ();
  if (n)
    f1 ();
  n = 1;
 lab:
  n++;
}
