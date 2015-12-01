#include <stdio.h>

int main (int argc, char** argv)
{
	int view_a[2];
	int view_b[2];
	int i, x __attribute__((stream));

	#pragma omp task output (x << view_a[2])
	{
		view_a[0] = 42;
		view_a[1] = 43;
		printf ("Producer 1: write %d %d to stream.\n", view_a[0], view_a[1]); fflush (stdout);
	}

	for (i = 0; i < 10; ++i)
	{
		#pragma omp task firstprivate (i) output (x << view_a[2])
		{
			view_a[0] = 2*i;
			view_a[1] = 2*i + 1;
			printf ("Producer 2: write %d %d to stream.\n", view_a[0], view_a[1]); fflush (stdout);
		}

		if (i % 2) {
			#pragma omp task input (x >> view_b[2])
			{
				printf ("Consumer 1: read %d %d from stream.\n", view_b[0], view_b[1]); fflush (stdout);
			}
		} else {
			#pragma omp task input (x >> view_b[2])
			{
				printf ("Consumer 2: read %d %d from stream.\n", view_b[0], view_b[1]); fflush (stdout);
			}
		}
	}

	#pragma omp tick (x >> 2)

	return 0;
}
