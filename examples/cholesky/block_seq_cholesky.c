#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <cblas.h>
#include <getopt.h>
#include <string.h>
#include "../common/common.h"

#define _SPEEDUPS 1
#define _VERIFY 1

#include <unistd.h>

/* Missing declarations from liblapack */
int dlarnv_(long *idist, long *iseed, int *n, double *x);
void dpotrf_( unsigned char *uplo, int * n, double *a, int *lda, int *info );

void
stream_dpotrf (int block_size, int blocks,
	       double *blocked_data[blocks][blocks])
{
  int i, j, k;

  for (j = 0; j < blocks; ++j)
    {
      for (k = 0; k < j; ++k)
	{
	  for (i = j + 1; i < blocks; ++i)
	    {
		    cblas_dgemm (CblasColMajor, CblasNoTrans, CblasTrans,
				 block_size, block_size, block_size,
				 -1.0, blocked_data[i][k], block_size,
				 blocked_data[j][k], block_size,
				 1.0, blocked_data[i][j], block_size);
	    }
	}

      for (i = 0; i < j; ++i)
	{
		cblas_dsyrk (CblasColMajor, CblasLower, CblasNoTrans,
			     block_size, block_size,
			     -1.0, blocked_data[j][i], block_size,
			     1.0, blocked_data[j][j], block_size);
	}

      {
	      unsigned char lower = 'L';
	      int n = block_size;
	      int nfo;
	      dpotrf_(&lower, &n, blocked_data[j][j], &n, &nfo);
      }

      for (i = j + 1; i < blocks; ++i)
	{
		cblas_dtrsm (CblasColMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
			     block_size, block_size,
			     1.0, blocked_data[j][j], block_size,
			     blocked_data[i][j], block_size);
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
		      blocked_data[ii][jj][i*block_size + j], data[i*N+j]);
	      exit (1);
	    }
}



static void
blockify (int block_size, int blocks, int N,
	  double *data, double *blocked_data[blocks][blocks])
{
  int ii, i, jj;

  for (ii = 0; ii < blocks; ++ii)
    for (jj = 0; jj < blocks; ++jj)
      for (i = 0; i < block_size; ++i)
	memcpy (&blocked_data[ii][jj][i*block_size], &data[(ii*block_size + i)*N + jj*block_size],
		block_size * sizeof (double));
}


int
main(int argc, char *argv[])
{
  int option;
  int i, iter;
  int N = 4096;

  int numiters = 10;
  int block_size = 256;

  FILE *res_file = NULL;
  FILE *in_file = NULL;

  while ((option = getopt(argc, argv, "n:s:b:r:i:o:h")) != -1)
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
	case 'h':
	  printf("Usage: %s [option]...\n\n"
		 "Options:\n"
		 "  -n <size>                    Number of colums of the square matrix, default is %d\n"
		 "  -s <power>                   Set the number of colums of the square matrix to 1 << <power>\n"
		 "  -b <block size power>        Set the block size 1 << <block size power>\n"
		 "  -r <number of iterations>    Number of repetitions of the execution\n"
		 "  -i <input file>              Read matrix data from an input file\n"
		 "  -o <output file>             Write matrix data to an output file\n",
		 argv[0], N);
	  exit(0);
	  break;
	case '?':
	  fprintf(stderr, "Run %s -h for usage.\n", argv[0]);
	  exit(1);
	  break;
	}
    }

  if(optind != argc) {
	  fprintf(stderr, "Too many arguments. Run %s -h for usage.\n", argv[0]);
	  exit(1);
  }


  struct timeval *start = (struct timeval *) malloc (numiters * sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (numiters * sizeof (struct timeval));
  struct timeval *sstart = (struct timeval *) malloc (numiters * sizeof (struct timeval));
  struct timeval *send = (struct timeval *) malloc (numiters * sizeof (struct timeval));

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
      long int seed[4] = {1092, 43, 77, 1};
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


  for (iter = 0; iter < numiters; ++iter)
    {
      blockify (block_size, blocks, N, data, (void *)blocked_data);

      /* Only effectively can start once the previous task completes.  */
      gettimeofday (&start[iter], NULL);
      stream_dpotrf (block_size, blocks, (void *)blocked_data);
      gettimeofday (&end[iter], NULL);


    if (_SPEEDUPS)
      {
	unsigned char lower = 'L';
	int nfo;
	double stream_time = 0, seq_time = 0;

	stream_time = tdiff (&end[iter], &start[iter]);

	double * seq_data;
	if (posix_memalign ((void **) &seq_data, 64, size * sizeof (double)))
	  {
	    printf ("Out of memory.\n");
	    exit (1);
	  }
	memcpy (seq_data, data, size * sizeof (double));

	gettimeofday (&sstart[iter], NULL);
	dpotrf_(&lower, &N, seq_data, &N, &nfo);
	gettimeofday (&send[iter], NULL);

	seq_time = tdiff (&send[iter], &sstart[iter]);
	if (_VERIFY)
	  verify (block_size, N, blocks, seq_data, blocked_data);
	free (seq_data);
	//printf ("%.5f \t (seq: \t %.5f, str: \t %.5f)\n", seq_time/stream_time, seq_time, stream_time);
      }
    }

  double stream_time = 0, seq_time = 0;

  for (iter = 0; iter < numiters; ++iter)
    {
      stream_time += tdiff (&end[iter], &start[iter]);
      seq_time += tdiff (&send[iter], &sstart[iter]);
    }

  if (_SPEEDUPS)
    {
      printf ("%.5f \t (seq: \t %.5f, str: \t %.5f)\n", seq_time/stream_time, seq_time, stream_time);
    }
  else
    {
      printf ("%.5f \n", stream_time);
    }

  return 0;
}





