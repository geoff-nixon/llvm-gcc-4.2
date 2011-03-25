/* APPLE LOCAL file radar 6083129 byref escapes */
/* { dg-do run { target *-*-darwin[1-2][0-9]* } } */
/* { dg-options "-fblocks" } */
/* { dg-skip-if "" { powerpc*-*-darwin* } { "-m64" } { "" } } */

extern void abort(void);

static int count;
static void _Block_object_dispose(void * arg, int flag) {
	++count;
}

int main()
{
  {
    __block int O1;
    int i = 0;
    while (++i != 5)
    {
            __block int I1;
    }
    if (count != 4)
	abort();
  }
    return count - 5;
}

