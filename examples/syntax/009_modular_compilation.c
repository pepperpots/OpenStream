/* Modular compilation is possible, relying on attributes to specify
   which parameters of a function are streams.  The function can then
   be called with a stream as parameter, without any callsite
   annotation.

 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/* The first parameter of this function is marked as a stream, which
   allows to use it in streaming clauses (input/output/peek or in tick
   directives.  */
void
modular_streaming (int sout __attribute__ ((stream)), int i)
{
  int v[2];

#pragma omp task firstprivate (i) output (sout << v[2])
  {
    v[0] = i;
    v[1] = i*3;
    printf ("((outlined)) Producer sends: %d %d \n", v[0], v[1]); fflush (stdout);
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

  int x[arg1] __attribute__((stream));

  for (i = 0; i < 10; ++i)
    {
      int v[4];

      /* Pass the stream x[1] to the function containing the producer
	 task for this stream.  */
      modular_streaming (x[1], i);

      if (i%2)
	{
#pragma omp task input (x[1] >> v[4])
	  {
	    printf (" => Consumer receives: %d %d %d %d\n", v[0], v[1], v[2], v[3]); fflush (stdout);
	  }
	}
    }

  return 0;
}
