/* APPLE LOCAL file v7 merge */
/* Test the `vtrnQu8' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vtrnQu8 (void)
{
  uint8x16x2_t out_uint8x16x2_t;
  uint8x16_t arg0_uint8x16_t;
  uint8x16_t arg1_uint8x16_t;

  out_uint8x16x2_t = vtrnq_u8 (arg0_uint8x16_t, arg1_uint8x16_t);
}

/* { dg-final { scan-assembler "vtrn\.8\[ 	\]+\[qQ\]\[0-9\]+, \[qQ\]\[0-9\]+!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
