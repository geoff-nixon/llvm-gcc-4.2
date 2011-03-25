/* { dg-do compile } */
/* { dg-options "-fno-strict-overflow -O2 -fdump-tree-final_cleanup" } */
/* LLVM LOCAL test not applicable */
/* { dg-require-fdump "" } */

/* Source: Ian Lance Taylor.  Dual of strict-overflow-1.c.  */

/* We can only simplify the conditional when using strict overflow
   semantics.  */

int
foo (int i)
{
  return i - 5 < 10;
}

/* { dg-final { scan-tree-dump "-[ ]*5" "final_cleanup" } } */
/* { dg-final { cleanup-tree-dump "final_cleanup" } } */
