/* APPLE LOCAL file v7 merge */
/* Test the `vdupQ_nf32' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

 /* LLVM LOCAL */
void test_vdupQ_nf32 (float32_t arg0_float32_t)
{
  float32x4_t out_float32x4_t;

  out_float32x4_t = vdupq_n_f32 (arg0_float32_t);
}

/* { dg-final { scan-assembler "vdup\.32\[ 	\]+\[qQ\]\[0-9\]+, \[rR\]\[0-9\]+!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
