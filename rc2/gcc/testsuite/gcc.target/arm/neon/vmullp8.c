/* APPLE LOCAL file v7 merge */
/* Test the `vmullp8' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vmullp8 (void)
{
  poly16x8_t out_poly16x8_t;
  poly8x8_t arg0_poly8x8_t;
  poly8x8_t arg1_poly8x8_t;

  out_poly16x8_t = vmull_p8 (arg0_poly8x8_t, arg1_poly8x8_t);
}

/* { dg-final { scan-assembler "vmull\.p8\[ 	\]+\[qQ\]\[0-9\]+, \[dD\]\[0-9\]+, \[dD\]\[0-9\]+!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
