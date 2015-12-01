#include <stdio.h>
#include <stdlib.h>

int main (int argc, char** argv)
{
	int x[1] __attribute__((stream));

	for (int i = 0; i < 10; ++i) {
		int v[5];

		if (i%2) {
			/* Producer executes at odd indexes.  */
			#pragma omp task firstprivate (i) output (x[0] << v[3])
			{
				v[0] = i + 1;
				v[1] = i + 2;
				v[2] = i + 3;
				printf ("Producer for odd indexes sends: %d %d %d\n", i+1, i+2, i+3); fflush (stdout);
			}
		} else {
			/* Producer executes at even indexes.  */
			#pragma omp task firstprivate (i) output (x[0] << v[2])
			{
				v[0] = i;
				v[1] = i + 1;
				printf ("Producer for even indexes sends: %d %d\n", i, i+1); fflush (stdout);
			}
		}

		if (i%2) {
			/* Consumer executes at odd indexes.  It reads the data
			   produced by both producer tasks, with a one iteration lag
			   on the even index producer.  The data is interleaved
			   according to the control flow of the control program.  */
			#pragma omp task input (x[0] >> v[5])
			{
				printf ("Consumer receives %d %d %d %d %d\n", v[0], v[1], v[2], v[3], v[4]); fflush (stdout);
			}
		}
	}


	return 0;
}
