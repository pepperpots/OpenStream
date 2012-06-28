#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <cblas.h>
#include <getopt.h>

#define _WITH_OUTPUT 0
#define _SPEEDUPS 1

#include <sys/time.h>
#include <unistd.h>
double
tdiff (struct timeval *end, struct timeval *start)
{
  return (double)end->tv_sec - (double)start->tv_sec +
    (double)(end->tv_usec - start->tv_usec) / 1e6;
}

static inline bool
double_equal (double a, double b)
{
  return (abs (a - b) < 1e-9);
}


void
stream_dpotrf (int block_size, int blocks,
	       double *blocked_data[blocks][blocks],
	       int streams[] __attribute__((stream)),
	       int Rstreams[] __attribute__((stream)),
	       int counters[])
{
  int i, j, k;

  for (j = 0; j < blocks; ++j)
    {
      for (k = 0; k < j; ++k)
	{
	  for (i = j + 1; i < blocks; ++i)
	    {
	      int readers = counters[i*blocks + j];
	      int w_0, w_1, w_2, w_3, w_4, w_5;

	      counters[i*blocks + k] += 1;
	      counters[j*blocks + k] += 1;

	      if (readers == 0)
		{
#pragma omp task peek (streams[i*blocks + k] >> w_0, streams[j*blocks + k] >> w_1) \
  output (Rstreams[i*blocks + k] << w_2, Rstreams[j*blocks + k] << w_3)	\
  input (streams[i*blocks + j] >> w_4) output (streams[i*blocks + j] << w_5)
		  {
		    cblas_dgemm (CblasColMajor, CblasNoTrans, CblasTrans,
				 block_size, block_size, block_size,
				 -1.0, blocked_data[i][k], block_size,
				 blocked_data[j][k], block_size,
				 1.0, blocked_data[i][j], block_size);
		  }
		}
	      else
		{
		  int Rwin[readers];
		  counters[i*blocks + j] = 0;

#pragma omp task peek (streams[i*blocks + k] >> w_0, streams[j*blocks + k] >> w_1) \
  output (Rstreams[i*blocks + k] << w_2, Rstreams[j*blocks + k] << w_3)	\
  input (streams[i*blocks + j] >> w_4) output (streams[i*blocks + j] << w_5) \
  input (Rstreams[i*blocks + j] >> Rwin[readers])
		  {
		    cblas_dgemm (CblasColMajor, CblasNoTrans, CblasTrans,
				 block_size, block_size, block_size,
				 -1.0, blocked_data[i][k], block_size,
				 blocked_data[j][k], block_size,
				 1.0, blocked_data[i][j], block_size);
		  }
		}
	    }
	}

      for (i = 0; i < j; ++i)
	{
	  int readers = counters[j*blocks + j];
	  int w_0, w_1, w_2, w_3;

	  counters[j*blocks + i] += 1;

	  if (readers == 0)
	    {
#pragma omp task peek (streams[j*blocks + i] >> w_0) output (Rstreams[j*blocks + i] << w_1) \
  input (streams[j*blocks + j] >> w_2) output (streams[j*blocks + j] << w_3)
	      {
		cblas_dsyrk (CblasColMajor, CblasLower, CblasNoTrans,
			     block_size, block_size,
			     -1.0, blocked_data[j][i], block_size,
			     1.0, blocked_data[j][j], block_size);
	      }
	    }
	  else
	    {
	      int Rwin[readers];
	      counters[j*blocks + j] = 0;

#pragma omp task peek (streams[j*blocks + i] >> w_0) output (Rstreams[j*blocks + i] << w_1) \
  input (streams[j*blocks + j] >> w_2) output (streams[j*blocks + j] << w_3) \
  input (Rstreams[j*blocks + j] >> Rwin[readers])
	      {
		cblas_dsyrk (CblasColMajor, CblasLower, CblasNoTrans,
			     block_size, block_size,
			     -1.0, blocked_data[j][i], block_size,
			     1.0, blocked_data[j][j], block_size);
	      }
	    }
	}

      {
	int readers = counters[j*blocks + j];
	int w_0, w_1;

	if (readers == 0)
	  {
#pragma omp task input (streams[j*blocks + j] >> w_0) output (streams[j*blocks + j] << w_1)
	    {
	      unsigned char lower = 'L';
	      int n = block_size;
	      int nfo;
	      dpotrf_(&lower, &n, blocked_data[j][j], &n, &nfo);
	    }
	  }
	else
	  {
	    int Rwin[readers];
	    counters[j*blocks + j] = 0;

#pragma omp task input (streams[j*blocks + j] >> w_0) output (streams[j*blocks + j] << w_1) \
  input (Rstreams[j*blocks + j] >> Rwin[readers])
	    {
	      unsigned char lower = 'L';
	      int n = block_size;
	      int nfo;
	      dpotrf_(&lower, &n, blocked_data[j][j], &n, &nfo);
	    }
	  }
      }

      for (i = j + 1; i < blocks; ++i)
	{
	  int readers = counters[i*blocks + j];
	  int w_0, w_1, w_2, w_3;

	  counters[j*blocks + j] += 1;

	  if (readers == 0)
	    {
#pragma omp task peek (streams[j*blocks + j] >> w_0) output (Rstreams[j*blocks + j] << w_1) \
  input (streams[i*blocks + j] >> w_2) output (streams[i*blocks + j] << w_3)
	      {
		cblas_dtrsm (CblasColMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
			     block_size, block_size,
			     1.0, blocked_data[j][j], block_size,
			     blocked_data[i][j], block_size);
	      }
	    }
	  else
	    {
	      int Rwin[readers];
	      counters[i*blocks + j] = 0;

#pragma omp task peek (streams[j*blocks + j] >> w_0) output (Rstreams[j*blocks + j] << w_1) \
  input (streams[i*blocks + j] >> w_2) output (streams[i*blocks + j] << w_3) \
  input (Rstreams[i*blocks + j] >> Rwin[readers])
	      {
		cblas_dtrsm (CblasColMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
			     block_size, block_size,
			     1.0, blocked_data[j][j], block_size,
			     blocked_data[i][j], block_size);
	      }
	    }
	}
    }
}

static inline void
verify (int block_size, int N, int blocks,
	double *data, double *blocked_data[blocks][blocks])
{
  int ii, i, jj, j;

  for (ii = 0; ii < blocks; ++ii)
    for (jj = 0; jj < blocks; ++jj)
      for (i = 0; i < block_size; ++i)
	for (j = 0; j < block_size; ++j)
	  if (!double_equal (blocked_data[ii][jj][i*block_size + j],
			     data[(ii*block_size + i)*N + jj*block_size + j]))
	    {
	      printf ("Result mismatch: %f \t %f\n",
		      blocked_data[i/block_size][j/block_size][(i%block_size)*block_size+j%block_size], data[i*N+j]);
	      exit (1);
	    }
}



static void
blockify (int block_size, int blocks, int N,
	  double *data, double *blocked_data[blocks][blocks])
{
  int ii, i, jj, j;

  for (ii = 0; ii < blocks; ++ii)
    for (jj = 0; jj < blocks; ++jj)
      for (i = 0; i < block_size; ++i)
	memcpy (&blocked_data[ii][jj][i*block_size], &data[(ii*block_size + i)*N + jj*block_size],
		block_size * sizeof (double));
}


int
main(int argc, char *argv[])
{
  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));

  int option;
  int i, j, iter;
  int N = 64;

  int numiters = 10;
  int block_size = 8;

  FILE *res_file = NULL;
  FILE *in_file = NULL;
  bool sequential_p = false;

  while ((option = getopt(argc, argv, "n:s:b:r:i:o:p:")) != -1)
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
	case 'p':
	  sequential_p = atoi(optarg);
	  break;
	}
    }

  int size = N * N;
  int blocks = (N / block_size);
  int num_blocks = blocks * blocks;

  double * data;
  double **blocked_data;

  if (posix_memalign ((void **) &data, 64, size * sizeof (double)))
    {
      printf ("Out of memory.\n");
      exit (1);
    }

  // Generate random numbers or read from file
  if (in_file == NULL)
    {
      long int seed[4] = {0, 0, 0, 1};
      long int sp = 1;
      dlarnv_(&sp, seed, &size, data);

      // Also allow saving data sessions
      if (res_file != NULL)
	fwrite (data, sizeof (double), size, res_file);
    }
  else
    {
      fread (data, sizeof(double), size, in_file);
    }

  // Ensure matrix is definite positive
  for(i = 0; i < N; ++i)
    data[i*N + i] += N;

  // blocked matrix
  if (posix_memalign ((void **) &blocked_data, 64,
		      num_blocks * sizeof (double *)))
    {
      printf ("Out of memory.\n");
      exit (1);
    }
  for (i = 0; i < num_blocks; ++i)
    if (posix_memalign ((void **) &blocked_data[i], 64,
			block_size * block_size * sizeof (double)))
      {
	printf ("Out of memory.\n");
	exit (1);
      }
  blockify (block_size, blocks, N, data, (void *)blocked_data);

  int Rstreams[num_blocks] __attribute__((stream));
  int streams[num_blocks] __attribute__((stream));
  int final_view[num_blocks][1];

  int counters[num_blocks];

  for (i = 0; i < num_blocks; ++i)
    {
      int x;
      counters[i] = 0;

#pragma omp task output (streams[i] << x)
      x = 0;
    }


  gettimeofday (start, NULL);
  stream_dpotrf (block_size, blocks, (void *)blocked_data, streams, Rstreams, counters);
#pragma omp task input (streams >> final_view[num_blocks][1])
   {
     double stream_time, seq_time;
     unsigned char lower = 'L';
     int nfo;

     gettimeofday (end, NULL);
     stream_time = tdiff (end, start);

     /* Sequential LAPACK comparison. */
     gettimeofday (start, NULL);
     dpotrf_(&lower, &N, data, &N, &nfo);
     gettimeofday (end, NULL);
     seq_time = tdiff (end, start);

     verify (block_size, N, blocks, data, blocked_data);

     if (_SPEEDUPS)
       printf ("%.5f \t (seq: \t %.5f, str: \t %.5f)\n", seq_time/stream_time, seq_time, stream_time);
     else
       {
	 printf ("%.5f \n", stream_time);
       }
   }
}





