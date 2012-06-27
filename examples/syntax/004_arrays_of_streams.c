/* Illustrate the use of arrays of streams.  When streams are not
   scalars, we require that all stream accesses be made through an
   explicit access window, independently of the burst (i.e., even
   scalar accesses with burst=1 must use an access window).  */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

int
main (int argc, char **argv)
{
  int x[2] __attribute__((stream)); /* Array of streams.  */
  int i;

  for (i = 0; i < 10; ++i)
    {
      int scalar_window;    /* Scalar stream access window.  */
      int array_window[2];  /* Array stream access window.  */

#pragma omp task firstprivate (i) output (x[0] << scalar_window, x[1] << array_window[2])
      {
	scalar_window = i;
	array_window[0] = 2 * i;
	array_window[1] = 3 * i;
	printf ("Producer sends: %d on x[0] and (%d, %d) on x[1]\n", scalar_window, array_window[0], array_window[1]); fflush (stdout);
      }

#pragma omp task input (x[0] >> scalar_window, x[1] >> array_window[2])
      {
	printf (" => Consumer receives: %d on x[0] and (%d, %d) on x[1]\n", scalar_window, array_window[0], array_window[1]); fflush (stdout);
      }
    }


  return 0;
}
