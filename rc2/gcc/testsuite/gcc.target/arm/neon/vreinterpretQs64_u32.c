/* APPLE LOCAL file v7 merge */
/* Test the `vreinterpretQs64_u32' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vreinterpretQs64_u32 (void)
{
  int64x2_t out_int64x2_t;
  uint32x4_t arg0_uint32x4_t;

  out_int64x2_t = vreinterpretq_s64_u32 (arg0_uint32x4_t);
}

/* { dg-final { cleanup-saved-temps } } */
