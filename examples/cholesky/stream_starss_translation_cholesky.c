/* Author: Antoniu Pop <antoniu.pop@gmail.com>

   The objective of this code is to show how StarSs/OmpSs translation
   to OpenStream is possible in a very straightforward manner.  This
   only handles array regions, but can be applied to scalars as well
   when their semantics are also sequentially consistent (i.e., as the
   input/output/inout clauses do not have FIFO semantics).

   The approach is based on the idea that streams can directly encode
   the depdendences, as shown below, with 2 streams per memory region
   for all flow/anti/output dependences, and without
   over-synchronization.

   This code shows quite reasonable performance on a 4-core SMT
   Core-i7, getting close to linear (considering 8 HW threads).


   For the code generation, extra care is taken to ensure that the
   translation process requires as little work as possible.  In this
   version, it is reduced to a copy-paste replacement of the StarSs
   annotations with completely generic OpenStream code.  All the
   translation work is handled by the way the StarSs runtime must pass
   the dependence information back to OpenStream in the form of a
   "Selection arrays" coding pattern.  The generic code replacing any
   callsite annotation is:

	  int streams_peek[MAX_CONNECTIONS] __attribute__((stream_ref)); int num_peek;
	  int streams_in[MAX_CONNECTIONS]  __attribute__((stream_ref)); int num_in;
	  int streams_out[MAX_CONNECTIONS]  __attribute__((stream_ref)); int num_out;

	  resolve_dependences (reg_desc, <XXXXXX>, &streams_peek[0], &num_peek, &streams_in[0], &num_in, &streams_out[0], &num_out);

	  int peek_view[num_peek][1];
	  int in_view[num_in][1];
	  int out_view[num_out][1];

	  #pragma omp task peek (streams_peek >> peek_view[num_peek][0]) input (streams_in >> in_view[num_in][1]) output (streams_out << out_view[num_out][1])

   The only code generation requirement is to adjust the number
   <XXXXXX> of region descriptors (and the call to the dependence
   resolver).

   The runtime adjustments are as follows:
     - attach 2 streams and a counter to each region.

     - change the dependence resolution function to take as parameter
       one or more additional pointers used to return the "selection
       arrays" STREAMS_PEEK, STREAMS_IN and STREAMS_OUT, as well as
       their effective sizes.

     - change the dependence resolution function to fill these arrays
       with the streams attached to regions along the guidelines
       detailed below.
 */



#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <cblas.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include "../common/common.h"
#include "../common/sync.h"
#include "../common/lapack.h"

#define _SPEEDUPS 0
#define _VERIFY 0

#define MAX_CONNECTIONS 1000

#include <unistd.h>

/* Mockup implementation of region management, considering here that
   regions are perfectly matching.  */

typedef enum {
  INPUT,
  OUTPUT,
  INOUT
} region_type;

typedef struct region_descriptor {
  region_type type;
  int id;  // We use this as a generic identifier, here the position of the block in the matrix.

} region_descriptor_t, *region_descriptor_p;

typedef struct region {
  void *version_stream;

  void *Rstream;
  int Rcount;
} region_t, *region_p;

/* Global structure storing the set of regions.  */
region_p regions;

void
resolve_dependences (region_descriptor_p reg_desc_array, int num_reg_desc,
		     void **streams_peek, int *num_peek,
		     void **streams_in, int *num_in,
		     void **streams_out, int *num_out)
{
  int pos_peek = 0, pos_in = 0, pos_out = 0;
  int i, j;

  for (i = 0; i < num_reg_desc; ++i)
    {
      region_descriptor_p reg_desc = &reg_desc_array[i];
      region_p reg = &regions[reg_desc->id];  /* Region lookup.  */

      switch (reg_desc->type)
	{
	case INPUT:
	  // peek on the streams of all write regions overlapping this one (flow dependences: wait for completion of preceding write ops)
	  streams_peek[pos_peek++] = reg->version_stream;
	  // additionally produce a token on the corresponding Rstreams (anti dependences: register as a consumer, the overwriting
	  // producers will synchronize on these to wait for completion of preceding consumers before overwriting)
	  streams_out[pos_out++] = reg->Rstream;
	  reg->Rcount += 1;
	  break;

	case OUTPUT:
	  // consume the previous version on all overlapping write regions (output dependences: forces the producer to wait for
	  // the completion of all preceding overlapping write operations)
	  streams_in[pos_in++] = reg->version_stream;
	  // produce a new version of this region
	  streams_out[pos_out++] = reg->version_stream;

	  // to synchronize the anti dependences, we need to consume all the tokens produced by
	  // previous consumers on the Rstreams of all overlapping read regions
	  for (j = 0; j < reg->Rcount; ++j)
	    streams_in[pos_in++] = reg->Rstream;
	  reg->Rcount = 0;
	  break;

	case INOUT:
	  // merge the two above
	  // consume the previous version on all overlapping write regions (output and flow dependences)
	  streams_in[pos_in++] = reg->version_stream;
	  // produce a new version
	  streams_out[pos_out++] = reg->version_stream;

	  // No need to use Rstreams for the input, but we need to wait for previous consumers
	  for (j = 0; j < reg->Rcount; ++j)
	    streams_in[pos_in++] = reg->Rstream;
	  reg->Rcount = 0;
	  break;

	default:
	  assert (0);
	}
    }
  *num_peek = pos_peek;
  *num_in = pos_in;
  *num_out = pos_out;
}



/* OmpSs implementation of Cholesky
 * (from slides provided by Rosa Badia)

#pragma omp task inout ([TS][TS]A)
void spotrf (float *A);
#pragma omp task input ([TS][TS]T) inout ([TS][TS]B)
void strsm (float *T, float *B);
#pragma omp task input ([TS][TS]A,[TS][TS]B) inout ([TS][TS]C)
void sgemm (float *A, float *B, float *C);
#pragma omp task input ([TS][TS]A) inout ([TS][TS]C)
void ssyrk (float *A, float *C)

void Cholesky( float *A ) {
  int i, j, k;
  for (k=0; k<NT; k++) {
    spotrf (A[k*NT+k]) ;
    for (i=k+1; i<NT; i++)
      strsm (A[k*NT+k], A[k*NT+i]);
    // update trailing submatrix
    for (i=k+1; i<NT; i++) {
      for (j=k+1; j<i; j++)
	sgemm( A[k*NT+i], A[k*NT+j], A[j*NT+i]);
      ssyrk (A[k*NT+i], A[i*NT+i]);
    }
  }
}
*/

/* This follows the same algorithm structure, recalling the
   annotations above as call-site annotations.  Note that the code for
   each task's conversion is entirely "copy-paste" code generation
   (only needs unique variable names) except for the region
   descriptors, which is not OpenStream related.  */
void
stream_dpotrf (int block_size, int blocks,
	       double *blocked_data[blocks][blocks])
{
  int i, j, k;
  int a, b;

  for (j = 0; j < blocks; ++j)
    {
      {
	// #pragma omp task inout ([TS][TS]A)
	// spotrf (A[j*NT+j]) ;

	region_descriptor_t reg_desc[1];
	reg_desc[0].type = INOUT;
	reg_desc[0].id = j*blocks + j; // This is normally determined by the resolver
	int streams_peek[MAX_CONNECTIONS] __attribute__((stream_ref)); int num_peek;
	int streams_in[MAX_CONNECTIONS]  __attribute__((stream_ref)); int num_in;
	int streams_out[MAX_CONNECTIONS]  __attribute__((stream_ref)); int num_out;

	resolve_dependences (reg_desc, 1, &streams_peek[0], &num_peek, &streams_in[0], &num_in, &streams_out[0], &num_out);

	int peek_view[num_peek][1];
	int in_view[num_in][1];
	int out_view[num_out][1];

#pragma omp task peek (streams_peek >> peek_view[num_peek][0]) input (streams_in >> in_view[num_in][1]) output (streams_out << out_view[num_out][1])
	{
	  unsigned char lower = 'L';
	  int n = block_size;
	  int nfo;

	  dpotrf_(&lower, &n, blocked_data[j][j], &n, &nfo);
	}
      }

      for (i = j + 1; i < blocks; ++i)
	{
	  // #pragma omp task input ([TS][TS]T) inout ([TS][TS]B)
	  // strsm (A[j*NT+j], A[j*NT+i]);

	  region_descriptor_t reg_desc[2];
	  reg_desc[0].type = INPUT;
	  reg_desc[0].id = j*blocks + j;
	  reg_desc[1].type = INOUT;
	  reg_desc[1].id = j*blocks + i;
	  int streams_peek[MAX_CONNECTIONS] __attribute__((stream_ref)); int num_peek;
	  int streams_in[MAX_CONNECTIONS]  __attribute__((stream_ref)); int num_in;
	  int streams_out[MAX_CONNECTIONS]  __attribute__((stream_ref)); int num_out;

	  resolve_dependences (reg_desc, 2, &streams_peek[0], &num_peek, &streams_in[0], &num_in, &streams_out[0], &num_out);

	  int peek_view[num_peek][1];
	  int in_view[num_in][1];
	  int out_view[num_out][1];

#pragma omp task peek (streams_peek >> peek_view[num_peek][0]) input (streams_in >> in_view[num_in][1]) output (streams_out << out_view[num_out][1])
	  {
	    cblas_dtrsm (CblasColMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
			 block_size, block_size,
			 1.0, blocked_data[j][j], block_size,
			 blocked_data[j][i], block_size);
	  }
	}

      for (i = j + 1; i < blocks; ++i)
	{
	  for (k = j + 1; k < i; ++k)
	    {

	      // #pragma omp task input ([TS][TS]A,[TS][TS]B) inout ([TS][TS]C)
	      // sgemm( A[j*NT+i], A[j*NT+k], A[k*NT+i]);

	      region_descriptor_t reg_desc[3];
	      reg_desc[0].type = INPUT;
	      reg_desc[0].id = j*blocks + i;
	      reg_desc[1].type = INPUT;
	      reg_desc[1].id = j*blocks + k;
	      reg_desc[2].type = INOUT;
	      reg_desc[2].id = k*blocks + i;
	      int streams_peek[MAX_CONNECTIONS] __attribute__((stream_ref)); int num_peek;
	      int streams_in[MAX_CONNECTIONS]  __attribute__((stream_ref)); int num_in;
	      int streams_out[MAX_CONNECTIONS]  __attribute__((stream_ref)); int num_out;

	      resolve_dependences (reg_desc, 3, &streams_peek[0], &num_peek, &streams_in[0], &num_in, &streams_out[0], &num_out);

	      int peek_view[num_peek][1];
	      int in_view[num_in][1];
	      int out_view[num_out][1];

#pragma omp task peek (streams_peek >> peek_view[num_peek][0]) input (streams_in >> in_view[num_in][1]) output (streams_out << out_view[num_out][1])
	      {
		cblas_dgemm (CblasColMajor, CblasNoTrans, CblasTrans,
			     block_size, block_size, block_size,
			     -1.0, blocked_data[j][i], block_size,
			     blocked_data[j][k], block_size,
			     1.0, blocked_data[k][i], block_size);
	      }

	    }
	  // #pragma omp task input ([TS][TS]A) inout ([TS][TS]C)
	  // ssyrk (A[j*NT+i], A[i*NT+i]);

	  region_descriptor_t reg_desc[2];
	  reg_desc[0].type = INPUT;
	  reg_desc[0].id = j*blocks + i;
	  reg_desc[1].type = INOUT;
	  reg_desc[1].id = i*blocks + i;
	  int streams_peek[MAX_CONNECTIONS] __attribute__((stream_ref)); int num_peek;
	  int streams_in[MAX_CONNECTIONS]  __attribute__((stream_ref)); int num_in;
	  int streams_out[MAX_CONNECTIONS]  __attribute__((stream_ref)); int num_out;

	  resolve_dependences (reg_desc, 2, &streams_peek[0], &num_peek, &streams_in[0], &num_in, &streams_out[0], &num_out);

	  int peek_view[num_peek][1];
	  int in_view[num_in][1];
	  int out_view[num_out][1];

#pragma omp task peek (streams_peek >> peek_view[num_peek][0]) input (streams_in >> in_view[num_in][1]) output (streams_out << out_view[num_out][1])
	  {
	    cblas_dsyrk (CblasColMajor, CblasLower, CblasNoTrans,
			 block_size, block_size,
			 -1.0, blocked_data[j][i], block_size,
			 1.0, blocked_data[i][i], block_size);
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
	      fprintf (stderr, "Result mismatch: %5.10f \t %5.10f\n",
		      blocked_data[ii][jj][i*block_size + j], data[(ii*block_size + i)*N + jj*block_size + j]);
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
  int option;
  int i, j, iter;
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

  // Allocate streams and regions, then store the stream references in the regions.
  // This should be done dynamically for dynamic regions data structures by using stream allocator/deallocator calls where appropriate.
  int Rstreams[num_blocks] __attribute__((stream));
  int streams[num_blocks] __attribute__((stream));
  regions = malloc (num_blocks * sizeof (region_t));
  for (i = 0; i < num_blocks; ++i)
    {
      regions[i].version_stream = streams[i];
      regions[i].Rstream = Rstreams[i];
      regions[i].Rcount = 0;
    }

  // Run NUMITER iterations
  for (iter = 0; iter < numiters; ++iter)
    {
      // refresh blocked data
      blockify (block_size, blocks, N, data, (void *)blocked_data);

      // Generate an "initial version" for each version stream.
      // This can alternatively be done by only generating inputs on version streams of regions
      // that have already been "output" once.

      // In other words, in the tree of regions, a region only exists once it is accessed,
      // so the second solution should be used rather than forcibly introducing an initial version like here
      for (i = 0; i < num_blocks; ++i)
	{
	  int x;
#pragma omp task output (streams[i] << x)
	  x = 0;
	}

      /* Start computation code.  */
      gettimeofday (&start[iter], NULL);
      stream_dpotrf (block_size, blocks, (void *)blocked_data);


      // This is necessary to flush streams before the barrier.  In practice, a stream is
      // automatically flushed once it is deallocated, like at the end of a region's life span.
      // If regions are collected before passing the barrier, this will not be needed.
      // Otherwise, it can be fixed in the OpenStream runtime.
      for (i = 0; i < num_blocks; ++i)
	{
#pragma omp tick (streams[i] >> 1)

	  if (regions[i].Rcount != 0)
	    {
#pragma omp tick (Rstreams[i] >> regions[i].Rcount)

	      regions[i].Rcount = 0;
	    }
	}

      // Same taskwait after the call. Inlining the above
      // "stream_dpotrf" function is perfectly fine, therefore getting the same
      // structure with a taskwait after the loop nest.
#pragma omp taskwait
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

  // Aggregate the perf results.  WARNING: these perf results should
  // not be used for anything but early prototyping/evaluation purposes as
  // they are quite flawed.  Run the sequential version separately and
  // disable _SPEEDUPS for reliable streaming perf results
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
}




