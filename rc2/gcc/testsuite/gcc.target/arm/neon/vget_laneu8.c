/* APPLE LOCAL file v7 merge */
/* Test the `vget_laneu8' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vget_laneu8 (void)
{
  uint8_t out_uint8_t;
  uint8x8_t arg0_uint8x8_t;

  out_uint8_t = vget_lane_u8 (arg0_uint8x8_t, 1);
}

/* { dg-final { scan-assembler "vmov\.u8\[ 	\]+\[rR\]\[0-9\]+, \[dD\]\[0-9\]+\\\[\[0-9\]+\\\]!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
