/* Contributed by Nicola Pero - Fri Mar  9 19:39:15 CET 2001 */
#include <objc/objc.h>

/* Test redefining self */

@interface TestClass 
{
  Class isa;
}
+ (Class) class;
@end

@implementation TestClass
+ (Class) class
{
  self = Nil;

  return self;
}
#ifdef __NEXT_RUNTIME__                                   
+ initialize { return self; }
#endif
@end


int main (void)
{
  if ([TestClass class] != Nil)
    {
      abort ();
    }

  return 0;
}
