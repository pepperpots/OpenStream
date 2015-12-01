#include <stdio.h>

int main (int argc, char **argv)
{
	int i;
	int x __attribute__((stream)); /* Stream */

	for (i = 0; i < 16; ++i) {
		int u[2], v[2], w[4];

		#pragma omp task firstprivate (i) output (x << u[2])
		{
			u[0] = 2*i;
			u[1] = 2*i + 1;
			printf ("Producer at iteration %d sends: %d %d \n", i, u[0], u[1]); fflush (stdout);
		}

		if (!(i%2)) {
			#pragma omp task input (x >> v[2])
			{
				printf ("Consumer A at iteration %d receives: %d %d\n", i, v[0], v[1]); fflush (stdout);
			}
		}

		if (!(i%4))
		{
			#pragma omp task input (x >> w[4])
			{
				printf ("Consumer B at iteration %d receives: %d %d %d %d\n", i, w[0], w[1], w[2], w[3]); fflush (stdout);
			}
		}
	}

	return 0;
}
