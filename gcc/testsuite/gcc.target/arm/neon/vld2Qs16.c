/* APPLE LOCAL file v7 merge */
/* Test the `vld2Qs16' ARM Neon intrinsic.  */
/* This file was autogenerated by neon-testgen.  */

/* { dg-do assemble } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-save-temps -O0 -mfpu=neon -mfloat-abi=softfp" } */

#include "arm_neon.h"

void test_vld2Qs16 (void)
{
  int16x8x2_t out_int16x8x2_t;

  out_int16x8x2_t = vld2q_s16 (0);
}

/* { dg-final { scan-assembler "vld2\.16\[ 	\]+\\\{((\[dD\]\[0-9\]+-\[dD\]\[0-9\]+)|(\[dD\]\[0-9\]+, \[dD\]\[0-9\]+))\\\}, \\\[\[rR\]\[0-9\]+\\\]!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { scan-assembler "vld2\.16\[ 	\]+\\\{((\[dD\]\[0-9\]+-\[dD\]\[0-9\]+)|(\[dD\]\[0-9\]+, \[dD\]\[0-9\]+))\\\}, \\\[\[rR\]\[0-9\]+\\\]!?\(\[ 	\]+@\[a-zA-Z0-9 \]+\)?\n" } } */
/* { dg-final { cleanup-saved-temps } } */
