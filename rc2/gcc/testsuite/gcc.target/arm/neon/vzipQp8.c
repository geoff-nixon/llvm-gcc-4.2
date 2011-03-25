/* APPLE LOCAL file v7 merge */
/* Test the `vzipQp8' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vzipQp8 (void)
{
  poly8x16x2_t out_poly8x16x2_t;
  poly8x16_t arg0_poly8x16_t;
  poly8x16_t arg1_poly8x16_t;

  out_poly8x16x2_t = vzipq_p8 (arg0_poly8x16_t, arg1_poly8x16_t);
}

/* { dg-final { scan-assembler "vzip\.8\[ 	\]+\[qQ\]\[0-9\]+, \[qQ\]\[0-9\]+!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
