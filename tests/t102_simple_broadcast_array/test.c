#include <stdio.h>

#define SIZE 1024

int main (int argc, char **argv)
{
	int i, x __attribute__((stream));
	int v[SIZE];

	#pragma omp task output (x << v[SIZE])
	{
		int sum = 0;

		for(int j = 0; j < SIZE; j++) {
			v[j] = j;
			sum += j;
		}

		printf("Producer: write %d to stream.\n", sum); fflush (stdout);
	}


	for (i = 0; i < 10; ++i) {
		#pragma omp task peek (x >> v[SIZE]) firstprivate (i)
		{
			int sum = 0;

			for (int j = 0; j < SIZE; j++)
				sum += v[j];

			printf("Task %d: read %d from stream.\n", i, sum); fflush (stdout);
		}
	}

	#pragma omp tick (x >> SIZE)

	return 0;
}
