#include <stdio.h>

int
main (int argc, char **argv)
{
	int data_size = 1024;
	int chain_length = 10;

	long x[chain_length] __attribute__((stream));
	char v[data_size];

	#pragma omp task output(x[0] << v[data_size])
	{
		printf("Init task: v = %p\n", v);

		for(int i = 0; i < data_size; i++)
			v[i] = i;
	}

	for(int i = 0; i < chain_length-2; i++) {
		#pragma omp task inout_reuse(x[i] >> v[data_size] >> x[i+1])
		{
			printf("Middle task: v = %p\n", v);

			for(int i = 0; i < data_size; i++)
				v[i] = i;
		}
	}

	#pragma omp task input(x[chain_length-2] >> v[data_size])
	{
		printf("Terminal task: v = %p\n", v);
	}

	#pragma omp taskwait

	return 0;
}
