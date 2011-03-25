/* APPLE LOCAL file v7 merge */
/* Test the `vget_lows8' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vget_lows8 (void)
{
  int8x8_t out_int8x8_t;
  int8x16_t arg0_int8x16_t;

  out_int8x8_t = vget_low_s8 (arg0_int8x16_t);
}

/* { dg-final { scan-assembler "vmov\[ 	\]+\[dD\]\[0-9\]+, \[dD\]\[0-9\]+!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
