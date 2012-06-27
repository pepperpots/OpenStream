/* Example of the use of peek operations on scalar streams to enable
   broadcast patterns (single producer, multiple consumers).  This is
   achieved by using "peek" and "tick" operations, the former giving
   access to data within the stream similarly to an "input" clause
   (albeit without consuming the data) and the latter allowing to
   advance the position in the stream, discarding data.

   Note that peek/tick are only provided as "syntactic sugar", which
   can be expressed with the more classical input/output clauses,
   though with a heavier syntax:

   #pragma omp task peek (x)
     { ... }
   <=>
   int window[1]; // windowed access
   #pragma omp task input (x >> window[0]) // 0-burst access
     { ... }

   #pragma omp tick (x)
   <=>
   #pragma omp task input (x)
     {;} // codeless task

*/

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
      /* Producer task.  */
#pragma omp task firstprivate (i) output (x)
      {
	x = i;
	printf ("Task 1: write %d to stream.\n", i); fflush (stdout);
      }

      /* Consumer tasks: as the "input" clause has been replaced with
	 a "peek" clause, this means that while the horizon is still
	 1, the burst is 0.  In other words, no data is consumed
	 within the stream by these tasks, each subsequent read
	 operation on stream X will see the same data elements.  */
#pragma omp task peek (x)
      {
	printf (" => Task 2-1: read from stream %d.\n", x); fflush (stdout);
      }

      /* Second consumer always sees the same data as the first.  */
#pragma omp task peek (x)
      {
	printf (" => Task 2-2: read from stream %d.\n", x); fflush (stdout);
      }

      /* Tick operation: advance the read position, discarding 1
	 element from the stream.  The next peek/input operation will
	 no longer see the discarded data item.  This syntax uses an
	 implicit "tick burst" of one element.  It is possible to use
	 a heavier syntax for multiple elements: tick (x >> k) will
	 discard k elements from the stream.  */
#pragma omp tick (x)
    }

  return 0;
}
