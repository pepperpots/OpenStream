#include <stdio.h>

int main (int argc, char **argv)
{
	int i, x __attribute__((stream));

	#pragma omp task output (x)
	{
		x = 12345;
		printf("Producer: write %d to stream.\n", x); fflush (stdout);
	}


	for (i = 0; i < 10; ++i) {
		#pragma omp task peek (x) firstprivate (i)
		{
			printf("Task %d: read %d from stream.\n", i, x); fflush (stdout);
		}
	}

	#pragma omp tick (x)

	return 0;
}
