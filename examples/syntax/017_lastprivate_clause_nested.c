/* This example illustrates the usage of the lastprivate clause within
   other tasks.

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
  int arg3 = 7;

  printf ("Initial values:  %d %d %d\n", arg1, arg2, arg3); fflush (stdout);

#pragma omp task firstprivate (arg1, arg2) lastprivate (arg3)
  {

    printf ("Values before task T1:  %d %d %d\n", arg1, arg2, arg3); fflush (stdout);
#pragma omp task firstprivate (arg1) lastprivate (arg3)
    {
      arg3 = arg1;
      printf ("Setting arg3 = %d \n\n", arg3); fflush (stdout);
    }
    printf ("Values after task T1:  %d %d %d\n\n", arg1, arg2, arg3); fflush (stdout);

#pragma omp task firstprivate (arg2) lastprivate (arg3)
    {
      arg3 = arg2;
    }
    printf ("Values after task T2:  %d %d %d\n\n", arg1, arg2, arg3); fflush (stdout);

  }

  printf ("Final values:  %d %d %d\n\n", arg1, arg2, arg3); fflush (stdout);

  return 0;
}
