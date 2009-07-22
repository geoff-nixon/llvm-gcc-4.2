/* APPLE LOCAL file v7 merge */
/* Test the `vreinterpretQu16_u8' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vreinterpretQu16_u8 (void)
{
  uint16x8_t out_uint16x8_t;
  uint8x16_t arg0_uint8x16_t;

  out_uint16x8_t = vreinterpretq_u16_u8 (arg0_uint8x16_t);
}

/* { dg-final { cleanup-saved-temps } } */
