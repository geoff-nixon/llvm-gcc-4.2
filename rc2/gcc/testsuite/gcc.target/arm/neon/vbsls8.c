/* APPLE LOCAL file v7 merge */
/* Test the `vbsls8' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vbsls8 (void)
{
  int8x8_t out_int8x8_t;
  uint8x8_t arg0_uint8x8_t;
  int8x8_t arg1_int8x8_t;
  int8x8_t arg2_int8x8_t;

  out_int8x8_t = vbsl_s8 (arg0_uint8x8_t, arg1_int8x8_t, arg2_int8x8_t);
}

/* { dg-final { scan-assembler "((vbsl)|(vbit)|(vbif))\[ 	\]+\[dD\]\[0-9\]+, \[dD\]\[0-9\]+, \[dD\]\[0-9\]+!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
