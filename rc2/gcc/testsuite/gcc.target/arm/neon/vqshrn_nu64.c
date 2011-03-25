/* APPLE LOCAL file v7 merge */
/* Test the `vqshrn_nu64' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vqshrn_nu64 (void)
{
  uint32x2_t out_uint32x2_t;
  uint64x2_t arg0_uint64x2_t;

  out_uint32x2_t = vqshrn_n_u64 (arg0_uint64x2_t, 1);
}

/* { dg-final { scan-assembler "vqshrn\.u64\[ 	\]+\[dD\]\[0-9\]+, \[qQ\]\[0-9\]+, #\[0-9\]+!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
