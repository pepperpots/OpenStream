/* This second example for the modular compilation shows two
   additional features:

   1. Stream parameters can be passed anonymously (without a fully
   typed stream) by relying on a "void *" type parameter.  The
   attribute annotation is optional but recommended for readability.

   2. When a stream or array of streams are not used in streaming
   constructs in the context of their declaration, we require a
   declaration-site attribute annotation.  This is necessary whenever
   a stream is declared for the sole purpose of being passed as
   parameter to functions containing the streaming code.

*/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

void
modular_streaming_prod (int sout __attribute__ ((stream)), int i)
{
  int v[4];

#pragma omp task firstprivate (i) output (sout << v[2])
  {
    v[0] = i;
    v[1] = i*3;
    printf ("((outlined)) Producer sends: %d %d\n", v[0], v[1]); fflush (stdout);
  }
}

void
modular_streaming_cons (int sin __attribute__ ((stream)))
{
  int v[4];

#pragma omp task input (sin >> v[4])
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

      modular_streaming_prod (x[1], i);

      if (i%2)
	{
	  modular_streaming_cons (x[1]);
	}
    }

  return 0;
}
