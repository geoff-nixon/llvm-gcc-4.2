/* Simple check that sibling calls are performed from a
   void non-leaf-function taking no arguments calling itself.

   Copyright (C) 2002 Free Software Foundation Inc.
   Contributed by Hans-Peter Nilsson  <hp@bitrange.com>  */

/* { dg-do run } */
/* APPLE LOCAL 5886557 explicitly disable inlining. */
/* { dg-options "-O2 -foptimize-sibling-calls -fno-inline-functions" } */

/* The option -foptimize-sibling-calls is the default, but serves as
   marker.  Self-recursion tail calls are optimized for all targets,
   regardless of presence of sibcall patterns.  */

extern void abort (void);
extern void exit (int);

extern void recurser_void (void);
extern void track (void);

int n = 0;
int main ()
{
  recurser_void ();
  exit (0);
}

void
recurser_void (void)
{
  if (n == 0 || n == 7)
    track ();

  if (n == 10)
    return;
  n++;
  recurser_void ();
}

void *trackpoint;

void
track ()
{
  char stackpos[1];

  if (n == 0)
    trackpoint = stackpos;
  else if (n != 7 || trackpoint != stackpos)
    abort ();
}
