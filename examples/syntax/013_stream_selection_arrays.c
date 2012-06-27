/* This coding pattern complements the variadic clauses from example
   012 and removes the expressiveness limitations mentioned in that
   example.  The essential idea is that it is possible to create
   arrays of stream references where we can copy, and of course
   reorder, streams from any other array or scalar streams, which
   allows to build arrays of streams that can be contiguously matched
   by an array of windows.

   The example at hand is almost identical with example 012, the only
   difference is that we would like to connect the two tasks through
   streams x[2] and x[0] rather than the previous x[0] and x[1].  To
   this effect, we define an array of stream descriptors of type "void
   *", then copy, and permutate, the references to the original
   streams from array X.

   The matching between the arrays of windows and the array of streams
   is still occurring on the first contiguous elements, but this time
   the first two streams being matched in array "stream_descriptors"
   are x[2] and x[0].

*/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

void
modular_streaming_prod (int sout[] __attribute__((stream)), int i)
{
  int v[2][2];

#pragma omp task firstprivate (i) output (sout << v[2][2])
  {
    v[0][0] = i;
    v[0][1] = i*2;
    v[1][0] = i*3;
    v[1][1] = i*4;
    printf ("Producer sends: (%d, %d) on stream sout[0] and (%d, %d) on sout[1].\n",
	    v[0][0], v[0][1], v[1][0], v[1][1]); fflush (stdout);
  }
}

void
modular_streaming_cons (int sin[] __attribute__((stream)))
{
  int v[2][4];

#pragma omp task input (sin >> v[2][4])
  {
    printf (" => Consumer receives:  (%d, %d, %d, %d) on stream sin[0] and (%d, %d, %d, %d) on stream sin[1]\n",
	    v[0][0], v[0][1], v[0][2], v[0][3],
	    v[1][0], v[1][1], v[1][2], v[1][3]); fflush (stdout);
  }
}

int
main (int argc, char **argv)
{
  int i, x[3] __attribute__ ((stream));

  /* Declaration of an array of stream pointers, which can be used in
     the same way as an array of streams once initialized.  */
  int stream_descriptors[3] __attribute__((stream_ref));

  /* Initialization of the array of stream pointers with streams from
     the array of streams "x".  Here, we shuffle the streams to have
     x[2] and x[0] as the first two elements.  This allows to select
     any streams available and gather them in the array of pointers
     which can be passed to functions or used for stream
     communication.  */
  stream_descriptors[0] = x[2];
  stream_descriptors[1] = x[0];
  stream_descriptors[2] = x[1];

  for (i = 0; i < 10; ++i)
    {
      modular_streaming_prod (stream_descriptors, i);

      if (i%2)
	{
	  modular_streaming_cons (stream_descriptors);
	}
    }

  return 0;
}
