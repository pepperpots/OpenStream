/* Same example as 013, but we show a broadcast over multiple streams
   behaviour with a variadic peek clause.

   The consumer function, and its nested consumer task, use a peek
   operation instead of an input operation, which allows multiple
   calls to this function to see the same values in all streams of the
   array.  In order to advance in the streams, we need to use tick
   operations as before.

*/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>


void
nested_streaming_prod (float sout __attribute__ ((stream_ref)), int i)
{
  int v[1];

#pragma omp task firstprivate (i) output (sout << v[1])
  {
    v[0] = 42 + i;

    printf ("Producer sends: %d\n", v[0]); fflush (stdout);
  }
}

void
nested_streaming_cons (float sin[] __attribute__ ((stream_ref)), int i)
{
  int v[1];

#pragma omp task input (sin[i] >> v[1])
  {
    printf (" => Consumer receives:  %d\n", v[0]); fflush (stdout);
  }
}

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
  float x[arg1] __attribute__ ((stream));

#pragma omp task firstprivate (x) private (i)
  {
    for (i = 0; i < 10; ++i)
      {
	float sref __attribute__ ((stream_ref));
	sref = x[i];

#pragma omp task firstprivate (sref, i)
	{
	  printf (".... Scheduling producer:  %d\n", i); fflush (stdout);
	  nested_streaming_prod (sref, i);
	}
      }
  }

#pragma omp task firstprivate (x) private (i)
  {
    for (i = 0; i < 10; ++i)
      {
	printf (".... Scheduling consumer:  %d\n", i); fflush (stdout);
	nested_streaming_cons (x, i);
      }
  }

  return 0;
}
