/* APPLE LOCAL file radar 6133042 */
/* Check that ivars of block pointer types are scanned by GC */
/* { dg-do run } */
/* { dg-options "-ObjC -mmacosx-version-min=10.6 -fblocks -fobjc-gc -framework Foundation" { target *-*-darwin* } } */
/* { dg-require-effective-target ilp32 } */
/* { dg-require-effective-target objc_gc } */

#include <Foundation/Foundation.h>

@interface TestObject : NSObject {
    int (^getInt)(void);
    BOOL b1;
    int (^getFloat)(void);
}
@property(assign, readonly) int (^getInt)(void);
@end

@implementation TestObject
@dynamic getInt;
@end

int main(char *argc, char *argv[]) {
    const char *layout = (char *)class_getIvarLayout([TestObject self]);
    printf("layout is:\n");
    int cursor = 0;
    // we're looking for slot 1
    int seeking = 1;
    while (*layout) {
        int skip = (*layout) >> 4;
        int process = (*layout) & 0xf;
        printf("(%x) skip %d, process %d\n", (*layout), skip, process);
        cursor += skip;
        if ((cursor <= seeking) && ((cursor + process) > seeking)) {
            printf("Will scan desired %d element!\n", seeking);
            return 0;
        }
        cursor += process;
        ++layout;
    }
    printf("%s: ***failure, didn't scan slot %d\n", argv[0], seeking);
    return 1;
}
