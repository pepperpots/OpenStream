#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <getopt.h>
#include "../common/common.h"
#include "../common/sync.h"

#define _WITH_OUTPUT 0
#define _WITH_BINARY_OUTPUT 0

#include <unistd.h>

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
  int block_size = 0;

  FILE *res_file = NULL;

  int volatile res = 0;

  while ((option = getopt(argc, argv, "n:s:b:r:o:h")) != -1)
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
	case 'o':
	  res_file = fopen(optarg, "w");
	  break;
	case 'h':
	  printf("Usage: %s [option]...\n\n"
		 "Options:\n"
		 "  -n <size>                    Number of colums of the square matrix, default is %d\n"
		 "  -s <power>                   Set the number of colums of the square matrix to 1 << <power>\n"
		 "  -b <block size power>        Set the block size 1 << <block size power>, default is %d\n"
		 "  -r <iterations>              Number of iterations\n"
		 "  -o <output file>             Write data to output file, default is seidel.out\n",
		 argv[0], N, block_size);
	  exit(0);
	  break;
	case '?':
	  fprintf(stderr, "Run %s -h for usage.\n", argv[0]);
	  exit(1);
	  break;
	}
    }

  if (block_size == 0)
    block_size = N;

  if(optind != argc) {
	  fprintf(stderr, "Too many arguments. Run %s -h for usage.\n", argv[0]);
	  exit(1);
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
	data[N*i + j] = (double) ((i == 26 && j == 26) || (i == N-26 && j == N-26)) ? 500 : 0; //(i*7 +j*13) % 17;

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

    if (_WITH_BINARY_OUTPUT)
      {
	int dim = N-2;
	fwrite(&dim, sizeof(dim), 1, res_file);

	for (i = 1; i < N - 1; i++)
	  fwrite(&data[N * i + 1], (N-2)*sizeof(double), 1, res_file);
      }
  }

  return 0;
}
