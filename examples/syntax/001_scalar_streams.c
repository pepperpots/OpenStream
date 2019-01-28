/* Simple pipeline of two tasks, one producer and one consumer,
   communicating through stream X.  The access to the stream occurs
   directly through the stream variable (access windows are
   implicit).  */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

int
main (int argc, char **argv)
{
  int i, x __attribute__((stream));

  for (i = 0; i < 10; ++i)
    {
      /* Producer task: each activation of this task produces exactly
	 one integer on stream X.  As the window is implicit, the
	 burst is 1 (meaning that one base element, here an "int", is
	 produced by one activation).  */
#pragma omp task firstprivate (i) output (x) proc_bind (spread)
      {
	x = i+77;
	printf ("Task 1: write %d to stream.\n", i); fflush (stdout);
      }

      /* Consumer task: as the access window is implicit for the
	 consumer as well, it also has an implicit burst of 1, meaning
	 that 1 element of the stream is consumed by each activation
	 of the task, and its horizon is 1, which means that only one
	 element can be accessed by a task activation in the stream
	 (no "peek" capability).  */
#pragma omp task input (x) firstprivate (i) proc_bind (spread)
      {
	printf (" => Task 2: read from stream %d (%d).\n", x, i); fflush (stdout);
      }
    }

  return 0;
}
