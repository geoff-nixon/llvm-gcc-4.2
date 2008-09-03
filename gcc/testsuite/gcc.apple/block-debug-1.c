/* APPLE LOCAL file 5939894 */
/* Verify that the desired debugging type is generated for a structure
   member that is a pointer to a block.  */

/* { dg-do compile } */
/* { dg-options "-g -O0 -fblocks -dA" } */
/* { dg-final { scan-assembler "invoke_impl.*DW_AT_name" } } */

struct inStruct {
  void (^genericBlockPtr)();
} is;

