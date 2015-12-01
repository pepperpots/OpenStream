#include <stdio.h>

int main (int argc, char** argv)
{
	int chain_length = 10;
	int val = 12345;

	int x[chain_length] __attribute__((stream));
	int v[10];

	#pragma omp task output(x[0] << v[10])
	{
		printf("Init task: val = %d\n", val);
		v[0] = val;
	}

	for(int i = 0; i < chain_length-2; i++) {
		#pragma omp task inout_reuse(x[i] >> v[10] >> x[i+1])
		{
			printf("Middle task %d: val = %d\n", i, v[0]);
		}
	}

	#pragma omp task input(x[chain_length-2] >> v[10])
	{
		printf("Terminal task: val = %d\n", v[0]);
	}

	#pragma omp taskwait

	return 0;
}
