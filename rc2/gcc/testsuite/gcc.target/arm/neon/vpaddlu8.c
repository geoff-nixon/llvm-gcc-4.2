/* APPLE LOCAL file v7 merge */
/* Test the `vpaddlu8' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vpaddlu8 (void)
{
  uint16x4_t out_uint16x4_t;
  uint8x8_t arg0_uint8x8_t;

  out_uint16x4_t = vpaddl_u8 (arg0_uint8x8_t);
}

/* { dg-final { scan-assembler "vpaddl\.u8\[ 	\]+\[dD\]\[0-9\]+, \[dD\]\[0-9\]+!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
