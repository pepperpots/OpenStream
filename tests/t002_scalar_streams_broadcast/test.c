#include <stdio.h>

int main (int argc, char** argv)
{
	int i, x __attribute__((stream));

	for (i = 0; i < 10; ++i)
	{
		#pragma omp task firstprivate (i) output (x)
		{
			x = i;
			printf("Task %d: write %d to stream.\n", i, i); fflush (stdout);
		}

		#pragma omp task peek (x)
		{
			printf("Task %d.1: read %d from stream.\n", i, x); fflush (stdout);
		}

		#pragma omp task peek (x)
		{
			printf("Task %d.2: read %d from stream.\n", i, x); fflush (stdout);
		}

		#pragma omp tick (x)
	}

	return 0;
}
