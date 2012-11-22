/* This example illustrates the usage of streams of streams.

 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>


int
main (int argc, char **argv)
{
  int streams[16] __attribute__((stream));
  void *s_o_s __attribute__((stream));
  int i;

  for (i = 0; i < 16; ++i)
    {
      int v;

      /* Produce on data item on each stream in array STREAMS.  */
#pragma omp task firstprivate (i) output (streams[i] << v)
      v = i;

      /* Put streams from array STREAMS into the stream of streams
	 S_O_S in reverse order.  */
#pragma omp task firstprivate (i) output (s_o_s)
      s_o_s = streams[15 - i];
    }

  for (i = 0; i < 16; ++i)
    {
      void *view_stream;

      /* Produce on data item on each stream in array STREAMS.  */
#pragma omp task firstprivate (i) input (s_o_s >> view_stream)
      {
	int view_data, v1, v2;

#pragma omp task firstprivate (i) input (view_stream >> view_data)
	printf ("From stream %d reading %d\n", i, view_data); fflush (stdout);

#pragma omp taskwait
      }
    }

#pragma omp taskwait

  return 0;
}



