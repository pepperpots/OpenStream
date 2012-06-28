#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <getopt.h>
#include <sys/time.h>
#include <unistd.h>

#define _WITH_OUTPUT 0

double
tdiff (struct timeval *end, struct timeval *start)
{
  return (double)end->tv_sec - (double)start->tv_sec +
    (double)(end->tv_usec - start->tv_usec) / 1e6;
}


int
main (int argc, char **argv)
{
  int option;
  int i, j, k, l, iter;
  int N = 64;

  int numiters = 10;
  int block_size = 8;

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

  /* Add space for borders.  */
  N += 2;

  if (_WITH_OUTPUT && res_file == NULL)
    res_file = fopen("ompss_seidel.out", "w");


  {
    struct timeval start, end;

    size_t padding = (N-2)/2 - 1;
    size_t padded_size = N + 2 * padding;
    size_t final_size = sizeof (double) * padded_size * padded_size;

    double (*data)[padded_size];

    posix_memalign ((void**) &(data), final_size, final_size);

    for (i = 0; i < N; ++i)
      for (j = 0; j < N; ++j)
	data[i+padding][j+padding] = (double) ((i == 25 && j == 25) || (i == N-25 && j == N-25)) ? 500 : 0; //(i*7 +j*13) % 17;



    /***************************************/
    /************ begin kernel *************/
    /***************************************/
    gettimeofday (&start, NULL);
    for (iter = 0; iter < numiters; iter++)
      for (i = padding + 1; i < N - 1 + padding; i += block_size)
	for (j = padding + 1; j < N - 1 + padding; j += block_size)
	  {
	    int supi = i + block_size;
	    int supj = j + block_size;

#pragma omp task inout (data[i+1:supi-1][j+1:supj-1])			\
  input (data[i-1;1][j:supj-1], data[supi;1][j:supj-1], data[i:supi-1][j-1;1], data[i:supi-1][supj;1])
	    {
	      for (k = i; k < supi; ++k)
		for (l = j; l < supj; ++l)
		  //data[i+k][j+l] = 0.25 * (data[i+k-1][j+l] + data[i+k+1][j+l] + data[i+k][j+l-1] + data[i+k][j+l+1]);
		  data[k][l] = 0.2 * (data[k][l] + data[k-1][l] + data[k+1][l] + data[k][l-1] + data[k][l+1]);
	    }
	  }

#pragma omp taskwait

    gettimeofday (&end, NULL);
    /***************************************/
    /************  end kernel  *************/
    /***************************************/

    printf ("%.5f\n", tdiff (&end, &start));


    if (_WITH_OUTPUT)
      {
	printf ("[StarSs] Seidel (size %d, tile %d, iterations %d) executed in %.5f seconds\n",
		N - 2, block_size, numiters, tdiff (&end, &start));

	for (i = padding; i < N + padding; ++i)
	  {
	    for (j = padding; j < N + padding; ++j)
	      fprintf (res_file, "%f \t", data[i][j]);
	    fprintf (res_file, "\n");
	  }
      }
  }
}
