/* This example illustrates the fundamental stream access interleaving
   semantics of this streaming model.  Multiple producers and multiple
   consumers can use a single stream to communicate, interleaving
   their accesses to the stream according to the dynamic control flow
   of the "control program" (main program that spawns the tasks).  For
   a more in-depth explanation of this example (along with some
   figures describing the interleaving behaviour, please refer to
   [A. Pop and A. Cohen. Work-streaming Compilation of Futures.
   Workshop on Programming Language Approaches to Concurrency and
   Communication-cEntric Software (PLACES) 2012] which can be found
   at:

   http://places12.di.fc.ul.pt/PLACES-Proceedings.pdf/at_download/file

   The paper example has the following structure.  Note that tasks are
   enclosed in arbitrary control flow.

#pragma omp task output (x)                     // Task T1 (producer on X)
  x = ...;

for (i = 0; i < N; ++i) {
  int view_a[2], view_b[2];

  #pragma omp task output (x << view_a[2])      // Task T2 (producer on X)
    view_a[0] = ...; view_a[1] = ...;

  if (i % 2) {
    #pragma omp task input (x >> view_b[2])     // Task T3 (consumer on X)
      use (view_b[0], view_b[1]);
  }

  #pragma omp task input (x)                    // Task T4 (consumer on X)
    use (x);
}


  However, due to the limitations of the underlying data-flow model of
  execution, we need to restrict the expressiveness of our
  stream-computing model wrt. the burst and horizon values.  Indeed,
  in order to keep the DF runtime simple and much more importantly to
  avoid incurring copy overhead, we require that the producer/consumer
  groups use the same bursts, or that the consumer's horizon be an
  integer multiple of the producer's.  This forbids the use of
  "partial peek" operations (non-null burst value for a consumer, but
  horizon > burst).

  For this reason, the above example requires a slight adjustment to
  the burst of tasks T1 and T4.

  */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

int
main (int argc, char **argv)
{
  int view_a[2], view_b[2];
  int i, x __attribute__((stream));

  /* First producer adds 42 and 43 in stream X.  As this task is not
     within the loop, it will only execute once, therefore giving an
     example of the coding pattern used to introduce a delay on a
     stream.  */
#pragma omp task output (x << view_a[2]) proc_bind (spread)       // Task T1 (producer on X)
  {
    view_a[0] = 42;
    view_a[1] = 43;
    printf ("Producer 1: write %d %d to stream.\n", view_a[0], view_a[1]); fflush (stdout);
  }

  for (i = 0; i < 10; ++i)
    {

      /* Second producer task.  This task uses the explicit stream
	 access window syntax, which allows connecting a scalar or
	 array to a stream with a chevron << or >> operator, to gain
	 access to multiple data elements within the stream.  Note
	 that the declaration size of the window is used as the
	 horizon, while the clause specifies the burst.  Here both are
	 2 (output clauses do not support burst != horizon for
	 now).  */
#pragma omp task firstprivate (i) output (x << view_a[2]) proc_bind (spread)  // Task T2 (producer on X)
      {
	view_a[0] = 2*i;
	view_a[1] = 2*i + 1;
	printf ("Producer 2: write %d %d to stream.\n", view_a[0], view_a[1]); fflush (stdout);
      }

      if (i % 2)
	{
	  /* First consumer task.   */
#pragma omp task input (x >> view_b[2]) proc_bind (spread)         // Task T3 (consumer on X)
	  {
	    printf ("=> Consumer 1: reads %d %d from stream.\n", view_b[0], view_b[1]); fflush (stdout);
	  }
	}
      else
	{
#pragma omp task input (x >> view_b[2]) proc_bind (spread)                   // Task T4 (consumer on X)
	  {
	    printf ("=> Consumer 2: reads %d %d from stream.\n", view_b[0], view_b[1]); fflush (stdout);
	  }
	}
    }

  /* We currently force the use of all data produced in streams, so it
     is necessary to discard the last two elements.  */
#pragma omp tick (x >> 2)

  return 0;
}
