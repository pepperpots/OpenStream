#include <stdio.h>

void modular_streaming(int sout __attribute__ ((stream)), int i)
{
	int v[2];

	#pragma omp task firstprivate (i) output (sout << v[2])
	{
		v[0] = i;
		v[1] = i*3;
		printf ("((outlined)) Producer sends: %d %d\n", v[0], v[1]); fflush (stdout);
	}
}

int main (int argc, char** argv)
{
	int i;
	int arg1 = 12;
	int x[arg1] __attribute__((stream));

	for (i = 0; i < 10; ++i) {
		int v[4];
		modular_streaming (x[1], i);

		if (i%2) {
			#pragma omp task input (x[1] >> v[4])
			{
				printf ("Consumer receives: %d %d %d %d\n", v[0], v[1], v[2], v[3]); fflush (stdout);
			}
		}
	}

	return 0;
}
