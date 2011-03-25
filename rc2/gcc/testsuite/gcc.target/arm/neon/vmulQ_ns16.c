/* APPLE LOCAL file v7 merge */
/* Test the `vmulQ_ns16' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vmulQ_ns16 (void)
{
  int16x8_t out_int16x8_t;
  int16x8_t arg0_int16x8_t;
  int16_t arg1_int16_t;

  out_int16x8_t = vmulq_n_s16 (arg0_int16x8_t, arg1_int16_t);
}

/* { dg-final { scan-assembler "vmul\.i16\[ 	\]+\[qQ\]\[0-9\]+, \[qQ\]\[0-9\]+, \[dD\]\[0-9\]+\\\[\[0-9\]+\\\]!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
