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

#pragma omp taskwait

  return 0;
}
