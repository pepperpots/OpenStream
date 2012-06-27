/* A simpler version of the example 006_multi_prod_matching: the
   consumer executes every other iteration and consumes the data
   produced by the producer task during the current and *next*
   iterations.

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
      int v[4];

#pragma omp task firstprivate (i) output (x[1] << v[2])
      {
	v[0] = 2*i;
	v[1] = 2*i + 1;
	printf ("Producer for iteration %d sends: %d %d\n", i, v[0], v[1]); fflush (stdout);
      }

      if (i%2)
	{
#pragma omp task input (x[1] >> v[4])
	  {
	    printf (" => Consumer for iteration i receives: %d %d %d %d\n", v[0], v[1], v[2], v[3]); fflush (stdout);
	  }
	}
    }

  return 0;
}
