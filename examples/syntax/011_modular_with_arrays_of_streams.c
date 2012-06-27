/* Continuing on the modular compilation from examples 009 and 010,
   this test shows how arrays of streams can be passed as parameter to
   a function.  The same rules apply as for a single stream parameter:
   either the stream array is fully typed and requires an attribute
   annotation (see the producer function here) or a void ** parameter
   is used, which can optionally have an attribute annotation as
   well.  */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

void
modular_streaming_prod (int sout[] __attribute__ ((stream)), int i)
{
  int v[4];

#pragma omp task firstprivate (i) output (sout[2] << v[2])
  {
    v[0] = i;
    v[1] = i*3;
    printf ("((outlined)) Producer sends: %d %d\n", v[0], v[1]); fflush (stdout);
  }
}

void
modular_streaming_cons (int sin[] __attribute__ ((stream)))
{
  int v[4];

#pragma omp task input (sin[2] >> v[4])
  {
    printf ("((outlined)) Consumer receives: %d %d %d %d\n", v[0], v[1], v[2], v[3]); fflush (stdout);
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

  /* The attribute annotation is required here as we cannot infer from
     the context that this is an array of streams.  */
  int x[arg1] __attribute__ ((stream));

  for (i = 0; i < 10; ++i)
    {
      int v[4];

      modular_streaming_prod (x, i);

      if (i%2)
	{
	  modular_streaming_cons (x);
	}
    }

  return 0;
}
