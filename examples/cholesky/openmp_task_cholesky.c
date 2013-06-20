#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include "../common/common.h"
#include "../common/sync.h"
#include "../common/lapack.h"
#include <unistd.h>

#include <cblas.h>


#define _WITH_OUTPUT 0
#define _SPEEDUPS 0
#define _VERIFY 0

/* #include <unistd.h> */

/* #define FILE void */

/* #ifndef _STRUCT_TIMEVAL */
/* #define _STRUCT_TIMEVAL 1 */
/* struct timeval { */
/* 	time_t      tv_sec;     /\* seconds *\/ */
/* 	suseconds_t tv_usec;    /\* microseconds *\/ */
/* }; */
/* #endif */

/* extern void* stderr; */

/* extern int atoi(const char *nptr); */
/* extern FILE *fopen(const char *path, const char *mode); */
/* extern int printf(const char *format, ...); */
/* extern int fprintf(FILE *stream, const char *format, ...); */
/* extern void exit(int status); */
/* extern void *malloc(size_t size); */
/* extern double fabs(double x); */
/* extern int dlarnv_(long *idist, long *iseed, int *n, double *x); */
/* extern void dpotrf_( unsigned char *uplo, int * n, double *a, int *lda, int *info ); */
/* extern void *memcpy(void *dest, const void *src, size_t n); */
/* extern int posix_memalign(void **memptr, size_t alignment, size_t size); */
/* extern size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream); */
/* extern size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream); */
/* extern void free(void *ptr); */




void
c_dpotrf (int block_size, int blocks, double **blocked_data, int i, int j, int k)
{
  unsigned char upper = 'U';
  int n = block_size;
  int nfo;

  //fprintf (stderr, "dpotrf %d %d\n", j, j);
  dpotrf_(&upper, &n, blocked_data[j*blocks + j], &n, &nfo);
}

void
c_dtrsm (int block_size, int blocks, double **blocked_data, int i, int j, int k)
{
  cblas_dtrsm (CblasRowMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
	       block_size, block_size,
	       1.0, blocked_data[j*blocks + j], block_size,
	       blocked_data[i*blocks + j], block_size);
}


void
c_dgemm (int block_size, int blocks, double **blocked_data, int i, int j, int k)
{
  cblas_dgemm (CblasRowMajor, CblasNoTrans, CblasTrans,
	       block_size, block_size, block_size,
	       -1.0, blocked_data[i*blocks + k], block_size,
	       blocked_data[j*blocks + k], block_size,
	       1.0, blocked_data[i*blocks + j], block_size);
}

void
c_dsyrk (int block_size, int blocks, double **blocked_data, int i, int j, int k)
{
  cblas_dsyrk (CblasRowMajor, CblasLower, CblasNoTrans,
	       block_size, block_size,
	       -1.0, blocked_data[j*blocks + i], block_size,
	       1.0, blocked_data[j*blocks + j], block_size);
}

/* This follows the same algorithm structure, recalling the
   annotations above as call-site annotations.  Note that the code for
   each task's conversion is entirely "copy-paste" code generation
   (only needs unique variable names) except for the region
   descriptors, which is not OpenStream related.  */
void
stream_dpotrf (int block_size, int blocks,
	       double **blocked_data)
{
#pragma omp parallel
  {
#pragma omp single
    {
      int i, j, k;

     for (j = 0; j < blocks; ++j)
	{
	  for (k = 0; k < j; ++k)
	    {
	      for (i = j + 1; i < blocks; ++i)
		{
#pragma omp task
		  c_dgemm (block_size, blocks, blocked_data, i, j, k);
		}
#pragma omp taskwait
	    }

	  for (i = 0; i < j; ++i)
	    {
	      c_dsyrk (block_size, blocks, blocked_data, i, j, k);
	    }

	  c_dpotrf (block_size, blocks, blocked_data, i, j, k);

	  for (i = j + 1; i < blocks; ++i)
	    {
#pragma omp task
	      c_dtrsm (block_size, blocks, blocked_data, i, j, k);
	    }
#pragma omp taskwait
	}
    }
  }
}


static inline void
verify (int block_size, int N, int blocks,
	double *data, double **blocked_data)
{
  int ii, i, jj, j;

  for (ii = 0; ii < blocks; ++ii)
    for (jj = 0; jj < blocks; ++jj)
      for (i = 0; i < block_size; ++i)
	for (j = 0; j < block_size; ++j)
	  if (!double_equal (blocked_data[ii*blocks + jj][i*block_size + j],
			     data[(ii*block_size + i)*N + jj*block_size + j]))
	    {
	      fprintf (stderr, "Result mismatch: %5.10f \t %5.10f\n",
		      blocked_data[ii*blocks + jj][i*block_size + j], data[(ii*block_size + i)*N + jj*block_size + j]);
	      exit (1);
	    }
}



static void
blockify (int block_size, int blocks, int N,
	  double *data, double **blocked_data)
{
  int ii, i, jj;

  for (ii = 0; ii < blocks; ++ii)
    for (jj = 0; jj < blocks; ++jj)
      for (i = 0; i < block_size; ++i)
	memcpy (&blocked_data[ii*blocks + jj][i*block_size], &data[(ii*block_size + i)*N + jj*block_size],
		block_size * sizeof (double));
}

struct profiler_sync psync;

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

  struct timeval *start = (struct timeval *) malloc (numiters * sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (numiters * sizeof (struct timeval));
  struct timeval *sstart = (struct timeval *) malloc (numiters * sizeof (struct timeval));
  struct timeval *send = (struct timeval *) malloc (numiters * sizeof (struct timeval));

  int size;
  int blocks;
  int num_blocks;

  double * data;
  double **blocked_data;
  double stream_time = 0, seq_time = 0;

  PROFILER_NOTIFY_PREPARE(&psync);

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


  size = N * N;
  blocks = (N / block_size);
  num_blocks = blocks * blocks;

  // Allocate data array
  if (posix_memalign ((void **) &data, 64, size * sizeof (double)))
    { printf ("Out of memory.\n"); exit (1); }

  // Generate random numbers or read from file
  if (in_file == NULL)
    {
      long int seed[4] = {1092, 43, 77, 1};
      long int sp = 1;
      dlarnv_(&sp, seed, &size, data);

      if (res_file != NULL)
	fwrite (data, sizeof (double), size, res_file);
    }
  else
    fread (data, sizeof(double), size, in_file);
  for(i = 0; i < N; ++i)
    data[i*N + i] += N;

  // Allocate blocked matrix
  if (posix_memalign ((void **) &blocked_data, 64,
		      num_blocks * sizeof (double *)))
    { printf ("Out of memory.\n"); exit (1); }
  for (i = 0; i < num_blocks; ++i)
    if (posix_memalign ((void **) &blocked_data[i], 64,
			block_size * block_size * sizeof (double)))
      { printf ("Out of memory.\n"); exit (1); }

  // Run NUMITER iterations
  for (iter = 0; iter < numiters; ++iter)
    {
      // refresh blocked data
      blockify (block_size, blocks, N, data, (void *)blocked_data);


      /* Start computation code.  */
      gettimeofday (&start[iter], NULL);
      PROFILER_NOTIFY_RECORD(&psync);
      stream_dpotrf (block_size, blocks, (void *)blocked_data);
      PROFILER_NOTIFY_PAUSE(&psync);
      gettimeofday (&end[iter], NULL);
      PROFILER_NOTIFY_FINISH(&psync);

    if (_SPEEDUPS)
      {
	unsigned char upper = 'U';
	int nfo;
	double stream_time = 0, seq_time = 0;
	double * seq_data;

	stream_time = tdiff (&end[iter], &start[iter]);

	if (posix_memalign ((void **) &seq_data, 64, size * sizeof (double)))
	  {
	    printf ("Out of memory.\n");
	    exit (1);
	  }
	memcpy (seq_data, data, size * sizeof (double));

	gettimeofday (&sstart[iter], NULL);

	/* Even though we try to obtain the lower matrix, we tell dpotrf to
	 * calculate the upper matrix as this is a FORTRAN routine which
	 * calculates indices in column major mode.
	 */
	dpotrf_(&upper, &N, seq_data, &N, &nfo);
	gettimeofday (&send[iter], NULL);

	seq_time = tdiff (&send[iter], &sstart[iter]);
	if (_VERIFY)
	  verify (block_size, N, blocks, seq_data, blocked_data);
	free (seq_data);
	//printf ("%.5f \t (seq: \t %.5f, str: \t %.5f)\n", seq_time/stream_time, seq_time, stream_time);
      }
    }

  // Aggregate the perf results.  WARNING: these perf results should
  // not be used for anything but early prototyping/evaluation purposes as
  // they are quite flawed.  Run the sequential version separately and
  // disable _SPEEDUPS for reliable streaming perf results
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
