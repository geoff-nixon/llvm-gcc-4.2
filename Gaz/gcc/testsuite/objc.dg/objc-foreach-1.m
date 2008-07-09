/* APPLE LOCAL file radar 4294910 */
/* Syntax check for the new foreach statement. */
/* { dg-options "-mmacosx-version-min=10.5 -std=c99 " } */
/* { dg-do compile } */

typedef struct objc_class *Class;

typedef struct objc_object {
 Class isa;
} *id;


@interface MyList 
@end

@implementation MyList
- (unsigned int)countByEnumeratingWithState:(struct __objcFastEnumerationState *)state objects:(id *)items count:(unsigned int)stackcount
{
        return 0;
}
- (void)addObject:object {
}

@end

@interface MyList (BasicTest)
- (void)compilerTestAgainst;
@end
void BEGIN();
void INFORLOOP();
void END();
@implementation MyList (BasicTest)
- (void)compilerTestAgainst {

	BEGIN();
	for (id elem in (self)) 
	  if (elem)
	    INFORLOOP();

	END();
}
@end

