/* This example shows how multiple producer tasks can be matched to a
   single consumer task.  This is mostly of interest in the DF
   compilation case as it represents the only simple relaxation of the
   "matched bursts" rule.  Indeed, as the data is stored directly
   within the consumer task's data-flow frame, we can generate code
   where the producer directly writes within this frame as long as the
   data can be written contiguously.  If multiple producers generate
   precisely the amount of data used by one consumer, we can allow the
   behaviour and generate the proper offset within the consumer's
   frame for each independent producer.

   In this example, two producer tasks write to stream X, the first
   produces 3 data elements per activation, while the second produces
   2 elements.  As the consumer reads 5 elements per activation, there
   is no partial overlap.

*/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

int
main (int argc, char **argv)
{
  int i;
  int arg1 = 12, arg2 = 10;
  if (argc == 3)
    {
      arg1 = atoi (argv[1]);;
      arg2 = atoi (argv[2]);
    }

  int x[arg1] __attribute__((stream));

  for (i = 0; i < 10; ++i)
    {
      int v[5];

      if (i%2)
	{
	  /* Producer executes at odd indexes.  */
#pragma omp task firstprivate (i) output (x[0] << v[3])
	  {
	    v[0] = i + 1;
	    v[1] = i + 2;
	    v[2] = i + 3;
	    printf ("Producer for odd indexes sends: %d %d %d\n", i+1, i+2, i+3); fflush (stdout);
	  }
	}
      else
	{
	  /* Producer executes at even indexes.  */
#pragma omp task firstprivate (i) output (x[0] << v[2])
	  {
	    v[0] = i;
	    v[1] = i + 1;
	    printf ("Producer for even indexes sends: %d %d\n", i, i+1); fflush (stdout);
	  }
	}

      if (i%2)
	{
	  /* Consumer executes at odd indexes.  It reads the data
	     produced by both producer tasks, with a one iteration lag
	     on the even index producer.  The data is interleaved
	     according to the control flow of the control program.  */
#pragma omp task input (x[0] >> v[5])
	  {
	    printf (" => Consumer receives %d %d %d %d %d\n", v[0], v[1], v[2], v[3], v[4]); fflush (stdout);
	  }
	}
    }


  return 0;
}
