#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>


void
modular_streaming_prod (int sout[] __attribute__((stream)), int i)
{
	int v[3][2];

#pragma omp task firstprivate (i) output (sout << v[3][2])
	{
		v[0][0] = i;
		v[0][1] = i*2;
		v[1][0] = i*3;
		v[1][1] = i*4;
		v[2][0] = i*5;
		v[2][1] = i*6;
		printf ("Producer sends: (%d, %d) on stream sout[0], (%d, %d) on sout[1] and (%d, %d) on sout[2].\n",
			v[0][0], v[0][1], v[1][0], v[1][1], v[2][0], v[2][1]); fflush (stdout);
	}
}

void
modular_streaming_cons (int sin[] __attribute__((stream)))
{
	int v[3][4];

	/* Note here that the burst specified on the window is ignored for
	   peek clauses (always considered as 0).  The horizon (here 4)
	   determines how many elements can be read in each stream matched
	   by the array of windows.  */
#pragma omp task peek (sin >> v[3][0])
	{
		printf (" => Consumer receives:  (%d, %d, %d, %d) on stream sin[0], (%d, %d, %d, %d) on stream sin[1] and (%d, %d, %d, %d) on stream sin[2]\n",
			v[0][0], v[0][1], v[0][2], v[0][3],
			v[1][0], v[1][1], v[1][2], v[1][3],
			v[2][0], v[2][1], v[2][2], v[2][3]); fflush (stdout);
	}
}

int
main (int argc, char **argv)
{
	int i, x[3] __attribute__ ((stream));

	int streams[3] __attribute__((stream_ref));

	/* As in example 013, we use a selection array where we shuffle the
	   streams from the original array.  */
	streams[0] = x[2];
	streams[1] = x[0];
	streams[2] = x[1];


	for (i = 0; i < 10; ++i)
	{
		modular_streaming_prod (streams, i);

		if (i%2)
		{
			modular_streaming_cons (streams);
			modular_streaming_cons (streams);

			/* For now, we do not offer a variadic tick operation, so
			   each stream must be independently "ticked".  */
#pragma omp tick (streams[0] >> 4)
#pragma omp tick (streams[1] >> 4)
#pragma omp tick (streams[2] >> 4)
		}

	}

	#pragma omp taskwait

	return 0;
}
