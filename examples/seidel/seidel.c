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
  int block_size = 4;

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
	  block_size = 1 << atoi (optarg);
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
    res_file = fopen("seidel.out", "w");

  N += 2;

  {
    double *data = malloc (N * N * sizeof(double));
    struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
    struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));

    for (i = 0; i < N; ++i)
      for (j = 0; j < N; ++j)
	data[N*i + j] = (double) ((i == 25 && j == 25) || (i == N-25 && j == N-25)) ? 500 : 0; //(i*7 +j*13) % 17;

    gettimeofday (start, NULL);
    for (iter = 0; iter < numiters; iter++)
      for (i = 0; i < N - 2; i += block_size)
	for (j = 0; j < N - 2; j += block_size)
	  gauss_seidel (N, &data[N * i + j], block_size);

    gettimeofday (end, NULL);

    printf ("%.5f\n", tdiff (end, start));

    if (_WITH_OUTPUT)
      {
	printf ("[Sequential] Seidel (size %d, tile %d, iterations %d) executed in %.5f seconds\n",
		N - 2, block_size, numiters, tdiff (end, start));

	for (i = 0; i < N; ++i)
	  {
	    for (j = 0; j < N; ++j)
	      fprintf (res_file, "%f \t", data[N * i + j]);
	    fprintf (res_file, "\n");
	  }
      }
  }
}
