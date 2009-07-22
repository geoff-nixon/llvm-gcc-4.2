/* APPLE LOCAL file v7 merge */
/* Test the `vmull_laneu32' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vmull_laneu32 (void)
{
  uint64x2_t out_uint64x2_t;
  uint32x2_t arg0_uint32x2_t;
  uint32x2_t arg1_uint32x2_t;

  out_uint64x2_t = vmull_lane_u32 (arg0_uint32x2_t, arg1_uint32x2_t, 1);
}

/* { dg-final { scan-assembler "vmull\.u32\[ 	\]+\[qQ\]\[0-9\]+, \[dD\]\[0-9\]+, \[dD\]\[0-9\]+\\\[\[0-9\]+\\\]!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
