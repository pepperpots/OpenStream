/* This example shows the syntax for using variadic streaming clauses
   (variable number of streams used as input or output), relying on
   arrays of streams and on arrays of windows, which give access to
   data blocks within each stream of the array to which they are
   connected.

   Arrays of access windows have a second, outermost dimension which
   serves to index streams rather than data within a stream.  The size
   along this dimension determines the number of streams that will be
   matched.  The connecting array of streams can be larger than the
   array of windows, but only the first streams in the array, up to
   the number of windows used, will be actually connected.

   Note that the main restrictions are that:
   1. Streams within the array are matched as a contiguous range
   (i.e., cannot access "all streams at odd indexes" or ranges of
   streams that do not start at the beginning of the array).
   2. All of the streams matched by a variadic window must have the same
   horizon and the same burst.

   In the current example, an array X of 3 streams is passed as
   parameter to a producer and a consummer functions.  Each of these
   functions contains a task, connected to multiple streams within
   this array.

*/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

void
modular_streaming_prod (int sout[] __attribute__((stream)), int i)
{
  /* Variadic window connecting 2 streams with a horizon of 2
     elements.  */
  int v[2][2];

  /* Connection of an array of windows to an array of streams.  No
     subscript may be used on the array.  */
  /* Note here that the burst specified on the view is ignored for
     peek clauses (always considered as 0).  */
#pragma omp task firstprivate (i) output (sout << v[2][2])
  {
    v[0][0] = i;     /* First element read during this activation on stream sout[0].  */
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
  /* Variadic window connecting 2 streams with a horizon of 4
     elements.  */
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
  int i;

  /* The attribute annotation is required here as we cannot infer from
     the context that this is an array of streams.  */
  int x[3] __attribute__ ((stream));

  for (i = 0; i < 10; ++i)
    {
      modular_streaming_prod (x, i);

      if (i%2)
	{
	  modular_streaming_cons (x);
	}
    }

  return 0;
}
