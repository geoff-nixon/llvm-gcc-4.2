/* { dg-do compile } */

void f1(void)
{
  #pragma omp flush a	/* { dg-error "expected" } */
  #pragma omp flush (	/* { dg-error "expected identifier" } */
  #pragma omp flush (b	/* { dg-error "undeclared|expected|for each" } */
  #pragma omp flush (c d)	/* { dg-error "undeclared|expected" } */
  #pragma omp flush (e)		/* { dg-error "undeclared" } */
}
