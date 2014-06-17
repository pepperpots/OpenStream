#include <stdio.h>

int main (int argc, char **argv)
{
	int data_size = 1024;
	int num_refs = 2;
	int chain_length = 31;

	long x[num_refs*(chain_length+1)] __attribute__((stream));
	long x1_refs[num_refs] __attribute__((stream_ref));
	long x2_refs[num_refs] __attribute__((stream_ref));

	char v[num_refs][data_size];
	char w[num_refs][data_size];

	for(int i = 0; i < num_refs; i++) {
		x1_refs[i] = x[i];
	}

	#pragma omp task output(x1_refs << v[num_refs][data_size])
	{
		puts("Init task");

		for(int i = 0; i < num_refs; i++)
			memset(&v[i][0], 0, data_size);
	}

	for(int i = 0; i < chain_length; i++) {
		for(int j = 0; j < num_refs; j++) {
			x1_refs[j] = x[i*num_refs+j];
			x2_refs[j] = x[(i+1)*num_refs+j];
		}

		#pragma omp task inout_reuse(x1_refs >> v[num_refs][data_size] >> x2_refs)
		{
			puts("Middle task");

			for(int j = 0; j < num_refs; j++) {
				memset(&v[j][0], i, data_size);
			}
		}
	}

	for(int i = 0; i < num_refs; i++) {
		x2_refs[i] = x[(chain_length*num_refs)+i];
	}

	#pragma omp task input(x2_refs >> v[num_refs][data_size])
	{
		puts("End task");
	}

	#pragma omp taskwait

	return 0;
}
