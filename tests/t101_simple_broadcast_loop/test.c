#include <stdio.h>

int main (int argc, char **argv)
{
	int i, x __attribute__((stream));

	for(int j = 0; j < 10; j++) {
		#pragma omp task output (x)
		{
			x = j;
			printf("Producer %d: write %d to stream.\n", j, j); fflush (stdout);
		}


		for (i = 0; i < 10; ++i) {
			#pragma omp task peek (x) firstprivate (i)
			{
				printf("Task %d.%d: read %d from stream.\n", j, i, x); fflush (stdout);
			}
		}

		#pragma omp tick (x)
	}

	return 0;
}
