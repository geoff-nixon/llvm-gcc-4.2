/* APPLE LOCAL file radar 5831855 */
#import <Foundation/Foundation.h>
/* Test that a block pointer can be converted back-and-forth to 'id' in
   both initializsation and assignment without requiring a cast. */
/* { dg-do run } */
/* { dg-options "-mmacosx-version-min=10.5 -ObjC" { target *-*-darwin* } } */

void * _NSConcreteStackBlock;

typedef void (^blockATYPE)(void);

id myBlockFunc (void (^blockA)(void))
{
  return blockA;
}

blockATYPE MyBlockIdFunc (id id_arg)
{
  return id_arg;
}

int main(char *argc, char *argv[]) {
    void (^blockA)(void) = ^ { printf("hello\n"); };
    // an 'id' can be initialized wit a block.
    id aBlock = blockA;

    // a block should be assignable to an id
    aBlock = blockA;

    // an 'id' should be assignable to a block
    blockA = aBlock;

    blockA = myBlockFunc (^ { printf("hello\n"); });

    aBlock = MyBlockIdFunc (aBlock);

    return 0;
}
