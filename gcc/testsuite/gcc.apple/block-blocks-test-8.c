/* APPLE LOCAL file radar 5932809 */
/* Test the __byreg runtime features. */
/* { dg-options "-mmacosx-version-min=10.5 -ObjC -framework Foundation" { target i?86-*-darwin*  } } */
/* { dg-do run { target i?86-*-darwin* } } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */

#import <Foundation/Foundation.h>

void * _NSConcreteStackBlock;

int CalledRetain = 0;

id objc_msgSend(id target, SEL cmd) {
    printf("[%p %s] called\n", target, sel_getName(cmd));
    if (!strcmp(sel_getName(cmd), "retain")) {
        CalledRetain = 1;
    }
    return target;
}

@interface DumbObject {
@public
    id isa;
}
@end
id newDumbObject() {
    DumbObject *result = (DumbObject *)calloc(sizeof(DumbObject), 1);
    result->isa = objc_getClass("DumbObject");
    return result;
}

@implementation DumbObject
+ new {
    return newDumbObject();
}
+ (void) initialize { }
- forward:(SEL)msg :(long *)params {
    printf("forward:: with %s called\n", sel_getName(msg));
    return self;
}
@end

//#include <Block_private.h>

enum {
    BLOCK_NEEDS_FREE =        (1 << 24),
    BLOCK_HAS_COPY_DISPOSE =  (1 << 25),
    BLOCK_NO_COPY =           (1 << 26), // interim byref: no copies allowed
    BLOCK_IS_GC =             (1 << 27),
};

struct Block_basic {
    void *isa;
    int Block_flags;  // int32_t
    int Block_size; // XXX should be packed into Block_flags
    void (*Block_invoke)(void *);
    void (*Block_copy)(void *dst, void *src);
    void (*Block_dispose)(void *);
    //long params[0];  // generic space for const & byref hidden params, return value, variable on needs of course
};
struct Block_byref {
    //long reserved;
    struct Block_byref *forwarding;
    int flags;//refcount;
    int size;
    void (*byref_keep)(struct Block_byref *dst, struct Block_byref *src);
    void (*byref_destroy)(struct Block_byref *);
    // long shared[0];
};


int ByrefAssignCopy = 0;

void _Block_byref_assign_copy(void *destp, void *source) {
    struct Block_byref *src = (struct Block_byref *)source;
    ByrefAssignCopy = 1;
    // lets look at the Block_byref
    if ((src->flags & BLOCK_HAS_COPY_DISPOSE) == 0) {
        printf("byref data block does not have bit saying that copy/dispose helper routines are present\n");
        return;
    }
    printf("calling out to byref copy helper at %p\n", src->byref_keep);
    int junk[100];
    src->byref_keep((struct Block_byref *)junk, src);
}

void _Block_byref_release(void *source) {
    struct Block_byref *src = source;
    src->byref_destroy(src);
}

int main(int argc, char *argv[]) {
    id __byref dumbo = newDumbObject(); //[DumbObject new];
    void (^dummy)(void) = ^{ | dumbo | /* { dg-warning "has been deprecated in blocks" } */
        [dumbo self];
    };

    struct Block_basic *aBlock = (struct Block_basic *)(void *)dummy;
    if ((aBlock->Block_flags & BLOCK_HAS_COPY_DISPOSE) == 0) {
        printf("Block should have a NON_POD copy/destroy helpers and flags to say so, but doesn't\n");
        return 1;
    }

    char result[200];
    printf("calling out to copy support helper at %p\n", aBlock->Block_copy);
    (*aBlock->Block_copy)(result, aBlock); // do fixup

    // The copy/destroy helper should have had a callout  to _Block_byref_assign_copy for its byref block
    if (! ByrefAssignCopy) {
        printf("_Block_copy_assign not called\n");
        return 1;
    }

    // the copy/destroy helper of the byref should have done dst->object = [src->object retain]
    if (! CalledRetain) {
        printf("byref block support helper did not call retain\n");
        return 1;
    }

    (*aBlock->Block_dispose)(aBlock);

    return 0;
}
