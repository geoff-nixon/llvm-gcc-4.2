/* APPLE LOCAL file radar 5376125 */
/* Test for new objc option -Wdirect-ivar-access. */
/* { dg-options "-mmacosx-version-min=10.5 -Wdirect-ivar-access" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface MyObject {
@public
    id _myMaster;
    id _isTickledPink;
}
@property(retain) id myMaster;
@property(assign) id isTickledPink;
@end

@implementation MyObject

@synthesize myMaster = _myMaster;
@synthesize isTickledPink = _isTickledPink;

- (void) doSomething {
    _myMaster = _isTickledPink; /* { dg-warning "ivar \'_myMaster\' is being directly accessed" } */
			 	/* { dg-warning "ivar \'_isTickledPink\' is being directly accessed" "" { target *-*-* } 21 } */
}

@end

MyObject * foo ()
{
	MyObject* p;
        p.isTickledPink = p.myMaster;	// ok
	p->_isTickledPink = p->_myMaster; /* { dg-warning "ivar \'_myMaster\' is being directly accessed" } */
					  /* { dg-warning "ivar \'_isTickledPink\' is being directly accessed" "" { target *-*-* } 31 } */
	return p->_isTickledPink;	/* { dg-warning "ivar \'_isTickledPink\' is being directly accessed" } */	
}

