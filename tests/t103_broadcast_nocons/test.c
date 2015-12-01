#include <stdio.h>

int main (int argc, char **argv)
{
	int i, x __attribute__((stream));

	#pragma omp task output (x)
	{
		x = 12345;
		printf("Producer: write %d to stream.\n", x); fflush (stdout);
	}

	#pragma omp tick (x)

	return 0;
}
