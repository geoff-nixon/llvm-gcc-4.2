/* APPLE LOCAL file v7 merge */
/* Test the `vld2Qs8' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vld2Qs8 (void)
{
  int8x16x2_t out_int8x16x2_t;

  out_int8x16x2_t = vld2q_s8 (0);
}

/* LLVM LOCAL Change to expect one VLD2 with 4 registers */
/* { dg-final { scan-assembler "vld2\.8\[ 	\]+\\\{((\[dD\]\[0-9\]+-\[dD\]\[0-9\]+)|(\[dD\]\[0-9\]+, \[dD\]\[0-9\]+, \[dD\]\[0-9\]+, \[dD\]\[0-9\]+))\\\}, \\\[\[rR\]\[0-9\]+\\\]!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
