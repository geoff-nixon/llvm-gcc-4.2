/* APPLE LOCAL file radar 5831920 */
#import <Foundation/Foundation.h>
/* Test a property with block type. */
/* { dg-do run } */
/* { dg-options "-mmacosx-version-min=10.5 -ObjC" { target *-*-darwin* } } */

#include <stdio.h>

void * _NSConcreteStackBlock;

@interface TestObject {

}
@property(copy, readonly) int (^getIntCopy)(void);
@property(retain, readonly) int (^getIntRetain)(void);
@end



int main(char *argc, char *argv[]) {
    return 0;
}
