/* This example illustrates the basic usage of the lastprivate clause,
   testing usage withing the control program.

 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>


int
main (int argc, char **argv)
{
  int arg1 = 3;
  int arg2 = 5;
  int x __attribute__((stream));

  printf ("Initial values:  %d %d\n", arg1, arg2); fflush (stdout);
#pragma omp task firstprivate (arg1) lastprivate (arg2)
  {
    arg2 = arg1;
  }
  printf ("Values after task T1:  %d %d\n\n", arg1, arg2); fflush (stdout);

  arg1 = 7;
  arg2 = 42;

  printf ("Values before task T2:  %d %d\n", arg1, arg2); fflush (stdout);
#pragma omp task firstprivate (arg1) output (x)
  {
    x = arg1;
  }

#pragma omp task input (x) lastprivate (arg2)
  {
    arg2 = x;
  }
  printf ("Values after task T2:  %d %d\n\n", arg1, arg2); fflush (stdout);


  return 0;
}
