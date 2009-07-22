/* APPLE LOCAL file v7 merge */
/* Test the `vdup_ns16' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vdup_ns16 (void)
{
  int16x4_t out_int16x4_t;
  int16_t arg0_int16_t;

  out_int16x4_t = vdup_n_s16 (arg0_int16_t);
}

/* { dg-final { scan-assembler "vdup\.16\[ 	\]+\[dD\]\[0-9\]+, \[rR\]\[0-9\]+!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
