/* APPLE LOCAL file v7 merge */
/* Test the `vmlal_laneu16' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vmlal_laneu16 (void)
{
  uint32x4_t out_uint32x4_t;
  uint32x4_t arg0_uint32x4_t;
  uint16x4_t arg1_uint16x4_t;
  uint16x4_t arg2_uint16x4_t;

  out_uint32x4_t = vmlal_lane_u16 (arg0_uint32x4_t, arg1_uint16x4_t, arg2_uint16x4_t, 1);
}

/* { dg-final { scan-assembler "vmlal\.u16\[ 	\]+\[qQ\]\[0-9\]+, \[dD\]\[0-9\]+, \[dD\]\[0-9\]+\\\[\[0-9\]+\\\]!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
