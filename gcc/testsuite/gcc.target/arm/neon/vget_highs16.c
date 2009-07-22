/* APPLE LOCAL file v7 merge */
/* Test the `vget_highs16' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vget_highs16 (void)
{
  int16x4_t out_int16x4_t;
  int16x8_t arg0_int16x8_t;

  out_int16x4_t = vget_high_s16 (arg0_int16x8_t);
}

/* { dg-final { cleanup-saved-temps } } */
