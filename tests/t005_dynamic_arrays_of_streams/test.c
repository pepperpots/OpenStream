#include <stdio.h>
#include <stdlib.h>

int main (int argc, char** argv)
{
	int i;
	int nstreams = 12;
	int nperm = 10;

	/* Permutation arrays for integers in [0 .. nperm-1].  */
	int permutation1[nperm];
	int permutation2[nperm];

	for (i = 0; i < nperm; ++i) {
		permutation1[i] = (i+7) % nperm;
		permutation2[i] = (i+3) % nperm;
	}

	/* Dynamically sized array of streams.  */
	int x[nstreams] __attribute__ ((stream));

	for (i = 0; i < nperm; ++i)
	{
		/* Dynamic subscripts, used for selecting the stream to be
		   accessed within the array.  */
		int idx1 = permutation1[i];
		int idx2 = permutation2[i];

		/* Dynamically sized stream access windows.  */
		int v1[idx1 + 2];
		int v2[idx2 + 2];

		#pragma omp task firstprivate (i) output (x[idx1] << v1[idx1 + 2])
		{
			v1[idx1] = i;
			v1[idx1+1] = i*3;
			printf ("Producer called for idx = %d, i = %d, 3*i = %d\n", idx1, i, i*3); fflush (stdout);
		}

		#pragma omp task input (x[idx2] >> v2[idx2 + 2])
		{
			printf ("Consumer called for idx = %d, i = %d, 3*i = %d\n", idx2, v2[idx2], v2[idx2+1]); fflush (stdout);
		}
	}


	return 0;
}
