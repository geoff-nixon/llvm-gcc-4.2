/* APPLE LOCAL file v7 merge */
/* Test the `vqshrun_ns64' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vqshrun_ns64 (void)
{
  uint32x2_t out_uint32x2_t;
  int64x2_t arg0_int64x2_t;

  out_uint32x2_t = vqshrun_n_s64 (arg0_int64x2_t, 1);
}

/* { dg-final { scan-assembler "vqshrun\.s64\[ 	\]+\[dD\]\[0-9\]+, \[qQ\]\[0-9\]+, #\[0-9\]+!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
