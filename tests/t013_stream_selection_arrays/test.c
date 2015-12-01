#include <stdio.h>

void modular_streaming_prod (int sout[] __attribute__((stream)), int i)
{
	int v[2][2];

	#pragma omp task firstprivate (i) output (sout << v[2][2])
	{
		v[0][0] = i;
		v[0][1] = i*2;
		v[1][0] = i*3;
		v[1][1] = i*4;
		printf ("Producer sends: (%d, %d) on stream sout[0] and (%d, %d) on stream sout[1].\n",
			v[0][0], v[0][1], v[1][0], v[1][1]); fflush (stdout);
	}
}

void modular_streaming_cons (int sin[] __attribute__((stream)))
{
	int v[2][4];

	#pragma omp task input (sin >> v[2][4])
	{
		printf ("Consumer receives: (%d, %d, %d, %d) on stream sin[0] and (%d, %d, %d, %d) on stream sin[1].\n",
			v[0][0], v[0][1], v[0][2], v[0][3],
			v[1][0], v[1][1], v[1][2], v[1][3]); fflush (stdout);
	}
}

int main (int argc, char** argv)
{
	int i, x[3] __attribute__ ((stream));

	int stream_descriptors[3] __attribute__((stream_ref));

	stream_descriptors[0] = x[2];
	stream_descriptors[1] = x[0];
	stream_descriptors[2] = x[1];

	for (i = 0; i < 10; ++i) {
		modular_streaming_prod (stream_descriptors, i);

		if (i%2)
			modular_streaming_cons (stream_descriptors);
	}

	return 0;
}
