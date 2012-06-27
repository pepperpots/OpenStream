/* Illustrate the use of arrays of streams in a more dynamic setup.
   This feature requires C99 syntax, where arrays can be dynamically
   sized.

   In this example, we use two permutation arrays to subscript the
   arrays of streams.  Note that the streams accessed by the producer
   and consumer tasks are not the same in the same iteration, the
   communication occurs between different iterations.  Furthermore,
   the horizon and burst sizes of each access is entirely dynamic
   itself, but we must ensure that, *independently* for each stream in
   the array, the burst/horizon are matched between producers and
   consumers.

   In this example, as the subscript of the stream array is used to
   define the burst/horizon of all accesses in a given stream, this
   does indeed guarantee that they are matched for each stream within
   the array.

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

  /* Permutation arrays for integers in [0 .. arg2-1].  */
  int permutation1[arg2];
  int permutation2[arg2];
  for (i = 0; i < arg2; ++i)
    {
      permutation1[i] = (i+7) % arg2;
      permutation2[i] = (i+3) % arg2;
    }

  /* Dynamically sized array of streams.  */
  int x[arg1] __attribute__ ((stream));

  for (i = 0; i < arg2; ++i)
    {
      /* Dynamic subscripts, used for selecting the stream to be
	 accessed within the array.  */
      int idx1 = permutation1[i];
      int idx2 = permutation2[i];

      /* Dynamically sized stream access windows.  */
      int v1[idx1 + 2];
      int v2[idx2 + 2];

#pragma omp task firstprivate (i) output (x[idx1] << v1[idx1 + 2])
      {
	v1[idx1] = i;
	v1[idx1+1] = i*3;
	printf ("Producer called for idx = %d -- i = %d \n", idx1, i); fflush (stdout);
      }

#pragma omp task input (x[idx2] >> v2[idx2 + 2])
      {
	printf (" => Consumer called for idx = %d -- i = %d - 3*i = %d\n", idx2, v2[idx2], v2[idx2+1]); fflush (stdout);
      }
    }


  return 0;
}
