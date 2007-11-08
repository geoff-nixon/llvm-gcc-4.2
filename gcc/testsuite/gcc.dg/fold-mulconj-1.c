/* PR tree-optimization/23452 */
/* { dg-do compile } */
/* { dg-options "-O2 -ffast-math -fdump-tree-gimple" } */
/* LLVM LOCAL test not applicable */
/* { dg-require-fdump "" } */

_Complex double foo(_Complex double z)
{
  return z * ~z;
}

_Complex int bar(_Complex int z)
{
  return z * ~z;
}

/* { dg-final { scan-tree-dump-times "CONJ_EXPR" 0 "gimple" } } */
/* { dg-final { cleanup-tree-dump "gimple" } } */
