/* PR tree-optimization/19828 */
/* { dg-do compile } */
/* APPLE LOCAL 4875151 make sure PRE is off */
/* { dg-options "-O1 -fno-tree-pre -fdump-tree-lim-details" } */
/* LLVM LOCAL test not applicable */
/* { dg-require-fdump "" } */

int cst_fun1 (int) __attribute__((__const__));
int cst_fun2 (int) __attribute__((__const__));
int pure_fun1 (int) __attribute__((__pure__));
int pure_fun2 (int) __attribute__((__pure__));
int foo (void);

int xxx (void)
{
  int i, k = foo (), x = 0;

  for (i = 0; i < 100; i++)
    {
      x += cst_fun1 (k);
      x += pure_fun1 (k);

      if (k)
	{
	  x += cst_fun2 (k);
	  x += pure_fun2 (k);
	}
    }

  return x;
}

/* Calls to cst_fun1 and pure_fun1 may be moved out of the loop.
   Calls to cst_fun2 and pure_fun2 should not be, since calling
   with k = 0 may be invalid.  */

/* { dg-final { scan-tree-dump-times "Moving statement" 3 "lim" } } */
/* { dg-final { cleanup-tree-dump "lim" } } */
