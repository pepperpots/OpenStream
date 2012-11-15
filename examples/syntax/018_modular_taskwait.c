/* This example illustrates the usage of the lastprivate clause within
   other tasks.

 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

int
foo (int parm)
{
  int res;
  int *resp = &res;

#pragma omp task firstprivate (parm, resp)
  *resp = parm;

#pragma omp taskwait

  return res;
}


int
main (int argc, char **argv)
{
  int a = 3;
  int b = 7; int *bp = &b;
  int c;

#pragma omp task firstprivate (a, bp)
  {
    *bp = foo (a);
  }

  c = foo (42);

  printf ("Final values:  %d %d %d\n\n", a, b, c); fflush (stdout);

  return 0;
}
