/* { dg-do compile } */
/* { dg-options "-O2 -fdump-tree-vrp1" } */
/* LLVM LOCAL test not applicable */
/* { dg-require-fdump "" } */

int 
foo(int a)
{
  int z = a | 1;
  return z != 0;
}

/* VRP should optimize this to a trivial "return 1".   */
/* { dg-final { scan-tree-dump-times "return 1" 1 "vrp1" } } * /
/* { dg-final { cleanup-tree-dump "vrp1" } } */




