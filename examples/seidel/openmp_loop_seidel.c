#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <getopt.h>

#define _WITH_OUTPUT 0

#include <sys/time.h>
#include <unistd.h>

double
tdiff (struct timeval *end, struct timeval *start)
{
  return (double)end->tv_sec - (double)start->tv_sec +
    (double)(end->tv_usec - start->tv_usec) / 1e6;
}

void
gauss_seidel (int N, double a[N][N], int block_size)
{
  int i, j;

  for (i = 1; i <= block_size; i++)
    for (j = 1; j <= block_size; j++)
      a[i][j] = 0.2 * (a[i][j] + a[i-1][j] + a[i+1][j] + a[i][j-1] + a[i][j+1]);
}


int
main (int argc, char **argv)
{
  int option;
  int i, j, iter;
  int N = 64;

  int numiters = 10;
  int block_size = 8;
  int bs = 3;

  FILE *res_file = NULL;
  FILE *in_file = NULL;

  int volatile res = 0;

  while ((option = getopt(argc, argv, "n:s:b:r:i:o:")) != -1)
    {
      switch(option)
	{
	case 'n':
	  N = atoi(optarg);
	  break;
	case 's':
	  N = 1 << atoi(optarg);
	  break;
	case 'b':
	  bs = atoi (optarg);
	  block_size = 1 << bs;
	  break;
	case 'r':
	  numiters = atoi (optarg);
	  break;
	case 'i':
	  in_file = fopen(optarg, "r");
	  break;
	case 'o':
	  res_file = fopen(optarg, "w");
	  break;
	}
    }

  if (res_file == NULL)
    res_file = fopen("openmp_loop_seidel.out", "w");

  N += 2;

  {
    int num_blocks = (N - 2) >> bs;
    double *data = malloc (N * N * sizeof(double));
    int a;
    struct timeval start, end;

    for (i = 0; i < N; ++i)
      for (j = 0; j < N; ++j)
	data[N*i + j] = (double) ((i == 25 && j == 25) || (i == N-25 && j == N-25)) ? 500 : 0; //(i*7 +j*13) % 17;

    gettimeofday (&start, NULL);

    // Traverse the hyperplans
    for (a = 0; a < 2 * (num_blocks + numiters); ++a)
      {
	// Spawn the tasks within each plan
	int ij_bound = (a < num_blocks) ? a+1 : num_blocks;

#pragma omp parallel for schedule (dynamic) collapse (2) private (j, iter)
	  for (i = 0; i < ij_bound; ++i)
	    for (j = 0; j < ij_bound; ++j)
	      if (((i + j) & 1) == (a & 1))
		{
		  iter = (a - (i + j)) >> 1;

		  if (iter >= 0 && iter < numiters)
		    {
		      gauss_seidel (N, &data[N * (i << bs) + (j << bs)], block_size);
		    }
		}
      }

    gettimeofday (&end, NULL);

    printf ("%.5f\n", tdiff (&end, &start));

    if (_WITH_OUTPUT)
      {
	printf ("[OpenMP loop] Seidel (size %d, tile %d, iterations %d) executed in %.5f seconds\n",
		N - 2, block_size, numiters, tdiff (&end, &start));

	for (i = 0; i < N; ++i)
	  {
	    for (j = 0; j < N; ++j)
	      fprintf (res_file, "%f \t", data[N * i + j]);
	    fprintf (res_file, "\n");
	  }
      }
  }
}