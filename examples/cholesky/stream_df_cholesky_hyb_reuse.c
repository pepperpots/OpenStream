/**
 * Hybrid dataflow / shared memory implementation of blocked cholesky
 * factorization using inout_reuse.
 *
 * Copyright (C) 2014 Andi Drebes <andi.drebes@lip6.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <getopt.h>
#include <string.h>
#include "../common/common.h"
#include "../common/sync.h"
#include "cholesky_common.h"

#ifdef USE_MKL
  #include <mkl_cblas.h>
  #include <mkl_lapack.h>
#else
  #include <cblas.h>
  #include "../common/lapack.h"
#endif

#define _SPEEDUPS 0
#define _VERIFY 0

#include <unistd.h>

void* streams;
void* dfbarrier_stream;

int dgemm_base(int l, int blocks);
int dgemm_output_stream(int l, int blocks, int y, int stage);
int dgemm_lower_input_stream(int l, int blocks, int y, int stage);
int dgemm_self_input_stream(int l, int blocks, int y, int stage);
int dgemm_upper_input_stream(int l, int blocks, int y, int stage);
int dsyrk_base(int l, int blocks);
int dsyrk_left_input_stream(int l, int blocks, int stage);
int dsyrk_self_input_stream(int l, int blocks, int stage);
int dsyrk_output_stream(int l, int blocks, int stage);
int dpotrf_base(int l, int blocks);
int dpotrf_self_input_stream(int l, int blocks);
int dpotrf_output_stream(int l, int blocks);
int dtrsm_base(int l, int blocks);
int dtrsm_self_input_stream(int l, int blocks, int y);
int dtrsm_top_input_stream(int l, int blocks, int y);
int dtrsm_output_stream(int l, int blocks, int y);
int dpotrf_dtrsm_proxy_base(int l, int blocks);
int dpotrf_dtrsm_proxy_output_stream(int l, int blocks, int stage);
int dtrsm_dgemm_proxy_base(int l, int blocks);
int dtrsm_dgemm_proxy_output_stream(int l, int blocks, int y, int stage);
int dtrsm_dsyrk_proxy_base(int l, int blocks);
int dtrsm_dsyrk_proxy_output_stream(int l, int blocks, int y);
int num_streams_at_level(int l, int blocks);
int num_dgemm_streams_at_level(int l, int blocks);
int num_dsyrk_streams_at_level(int l, int blocks);
int num_dpotrf_streams_at_level(int l, int blocks);
int num_dtrsm_streams_at_level(int l, int blocks);
int num_dpotrf_dtrsm_proxy_streams_at_level(int l, int blocks);
int num_dtrsm_dgemm_proxy_streams_at_level(int l, int blocks);
int num_dtrsm_dsyrk_proxy_streams_at_level(int l, int blocks);

int final_input_stream(int id_x, int id_y, int blocks);
void create_terminal_task(double* data, int id_x, int id_y, int N, int block_size, int padding_elements);

void copy_block_from_global(double* global_data, double* block, int id_x, int id_y, int N, int block_size, int padding_elements)
{
  for(int y = 0; y < block_size; y++) {
    int global_y = id_y*block_size+y;
    memcpy(&block[y*block_size], &global_data[global_y*(N+padding_elements)+id_x*block_size], block_size*sizeof(double));
  }
}

void copy_block_to_global(double* global_data, double* block, int id_x, int id_y, int N, int block_size, int padding_elements)
{
  for(int y = 0; y < block_size; y++) {
    int global_y = id_y*block_size+y;
    memcpy(&global_data[global_y*(N+padding_elements)+id_x*block_size], &block[y*block_size], block_size*sizeof(double));
  }
}

int dgemm_base(int l, int blocks) {
  if(l == 0)
    return 0;
  else
    return num_streams_at_level(l-1, blocks);
}

int dgemm_output_stream(int l, int blocks, int y, int stage) {
  return dgemm_base(l, blocks) + (y-l-1)*(l) + stage;
}

int dgemm_self_input_stream(int l, int blocks, int y, int stage) {
  return dgemm_output_stream(l, blocks, y, stage-1);
}

int dgemm_upper_input_stream(int l, int blocks, int y, int stage) {
  return dtrsm_dgemm_proxy_output_stream(stage, blocks, l, y - l - 1);
}

int dgemm_lower_input_stream(int l, int blocks, int y, int stage)
{
  return dtrsm_dgemm_proxy_output_stream(stage, blocks, y,
	(blocks - 2 - stage) - (y - stage - 1) + (l - stage) - 1);
}

int dsyrk_base(int l, int blocks) {
  return dgemm_base(l, blocks) + num_dgemm_streams_at_level(l, blocks);
}

int dsyrk_left_input_stream(int l, int blocks, int stage) {
  return dtrsm_dsyrk_proxy_output_stream(stage, blocks, l);
}

int dsyrk_self_input_stream(int l, int blocks, int stage) {
  return dsyrk_output_stream(l, blocks, stage-1);
}

int dsyrk_output_stream(int l, int blocks, int stage) {
  return dsyrk_base(l, blocks) + stage;
}

int dpotrf_base(int l, int blocks) {
  return dsyrk_base(l, blocks) + num_dsyrk_streams_at_level(l, blocks);
}

int dpotrf_self_input_stream(int l, int blocks) {
  return dsyrk_output_stream(l, blocks, l-1);
}

int dpotrf_output_stream(int l, int blocks) {
  return dpotrf_base(l, blocks);
}

int dtrsm_base(int l, int blocks) {
  return dpotrf_base(l, blocks)+1;
}

int dtrsm_self_input_stream(int l, int blocks, int y)
{
  return dgemm_output_stream(l, blocks, y, l-1);
}

int dtrsm_top_input_stream(int l, int blocks, int y) {
    return dpotrf_dtrsm_proxy_output_stream(l, blocks, y - l - 1);
}

int dtrsm_output_stream(int l, int blocks, int y) {
  return dtrsm_base(l, blocks) + y - l - 1;
}

int num_dpotrf_dtrsm_proxy_streams_at_level(int l, int blocks) {
  return blocks - l - 1;
}

int dpotrf_dtrsm_proxy_base(int l, int blocks) {
  return dtrsm_base(l, blocks) + num_dtrsm_streams_at_level(l, blocks);
}

int dpotrf_dtrsm_proxy_output_stream(int l, int blocks, int stage) {
  return dpotrf_dtrsm_proxy_base(l, blocks) + stage;
}

int num_dtrsm_dgemm_proxy_streams_at_level(int l, int blocks)
{
  if(l == blocks - 1)
    return 0;

  return (blocks-l-2)*(blocks-l-1);
}

int dtrsm_dgemm_proxy_base(int l, int blocks)
{
  return dpotrf_dtrsm_proxy_base(l, blocks) + num_dpotrf_dtrsm_proxy_streams_at_level(l, blocks);
}

int dtrsm_dgemm_proxy_output_stream(int l, int blocks, int y, int stage)
{
  return dtrsm_dgemm_proxy_base(l, blocks) +
    (y - l - 1)*(blocks - l - 2) + stage;
}

int dtrsm_dsyrk_proxy_base(int l, int blocks)
{
  return dtrsm_dgemm_proxy_base(l, blocks) +
    num_dtrsm_dgemm_proxy_streams_at_level(l, blocks);
}

int dtrsm_dsyrk_proxy_output_stream(int l, int blocks, int y)
{
  return dtrsm_dsyrk_proxy_base(l, blocks) + y - l - 1;
}

int num_dtrsm_dsyrk_proxy_streams_at_level(int l, int blocks)
{
  return blocks-l-1;
}

int num_streams_at_level(int l, int blocks)
{
  int nstreams_upper;

  if(l > 0)
    nstreams_upper = num_streams_at_level(l-1, blocks);
  else
    nstreams_upper = 0;

  return nstreams_upper +
    num_dgemm_streams_at_level(l, blocks) +
    num_dsyrk_streams_at_level(l, blocks) +
    num_dpotrf_streams_at_level(l, blocks) +
    num_dtrsm_streams_at_level(l, blocks) +
    num_dpotrf_dtrsm_proxy_streams_at_level(l, blocks) +
    num_dtrsm_dgemm_proxy_streams_at_level(l, blocks) +
    num_dtrsm_dsyrk_proxy_streams_at_level(l, blocks);
}

void create_dgemm_task(double* input_data, double* work_data, int x, int blocks, int block_size, int y, int stage, int padding_elements)
{
  int upper_stream = dgemm_upper_input_stream(x, blocks, y, stage);
  int lower_stream = dgemm_lower_input_stream(x, blocks, y, stage);
  int self_stream = dgemm_self_input_stream(x, blocks, y, stage);
  int out_stream = dgemm_output_stream(x, blocks, y, stage);

  double upper_in[1];
  double lower_in[1];
  double self_in[block_size*block_size];
  double out[block_size*block_size];

  int N = block_size*blocks;

  int upper_in_global_x = stage;
  int upper_in_global_y = x;

  double* upper_in_global = &work_data[upper_in_global_y*block_size* (N+padding_elements)+upper_in_global_x*block_size];

  int lower_in_global_x = stage;
  int lower_in_global_y = y;

  double* lower_in_global = &work_data[lower_in_global_y*block_size * (N+padding_elements)+lower_in_global_x*block_size];


  if(stage == 0)
    {
        #pragma omp task input(streams[upper_stream] >> upper_in[1], \
		       streams[lower_stream] >> lower_in[1]) \
	  output(streams[out_stream] << out[block_size*block_size])
        {
	  copy_block_from_global(input_data, out, x, y, N, block_size, padding_elements);

	  cblas_dgemm (CblasRowMajor, CblasNoTrans, CblasTrans,
		       block_size, block_size, block_size,
		       -1.0, upper_in_global, N+padding_elements,
		       lower_in_global, N+padding_elements,
		       1.0, out, block_size);

	  if(stage+2 < x)
	    create_dgemm_task(input_data, work_data, x, blocks, block_size, y, stage+2, padding_elements);
	}
    } else  {
       #pragma omp task input(streams[upper_stream] >> upper_in[1], \
			      streams[lower_stream] >> lower_in[1]) \
	 inout_reuse(streams[self_stream] >> out[block_size*block_size] >> streams[out_stream])
      {
	cblas_dgemm (CblasRowMajor, CblasNoTrans, CblasTrans,
		     block_size, block_size, block_size,
		     -1.0, upper_in_global, N+padding_elements,
		     lower_in_global, N+padding_elements,
		     1.0, out, block_size);

	if(stage+2 < x)
	  create_dgemm_task(input_data, work_data, x, blocks, block_size, y, stage+2, padding_elements);
      }
    }
}

void create_dsyrk_task(double* input_data, double* work_data, int x, int blocks, int block_size, int stage, int padding_elements)
{
  int self_stream = dsyrk_self_input_stream(x, blocks, stage);
  int left_stream = dsyrk_left_input_stream(x, blocks, stage);
  int out_stream = dsyrk_output_stream(x, blocks, stage);

  double self_in[block_size*block_size];
  double left_in[1];
  double out[block_size*block_size];

  int N = block_size*blocks;

  int left_in_global_x = stage;
  int left_in_global_y = x;

  double* left_in_global = &work_data[left_in_global_y*block_size*(N+padding_elements)+left_in_global_x*block_size];

  if(stage == 0) {
    #pragma omp task input(streams[left_stream] >> left_in[1]) \
      output(streams[out_stream] << out[block_size*block_size])
    {
      copy_block_from_global(input_data, out, x, x, N, block_size, padding_elements);

      cblas_dsyrk (CblasRowMajor, CblasLower, CblasNoTrans,
		   block_size, block_size,
		   -1.0, left_in_global, N+padding_elements,
		   1.0, out, block_size);

      if(stage + 2 < x)
	create_dsyrk_task(input_data, work_data, x, blocks, block_size, stage+2, padding_elements);
    }
  } else {
    #pragma omp task input(streams[left_stream] >> left_in[1]) \
      inout_reuse(streams[self_stream] >> out[block_size*block_size] >> streams[out_stream])
    {
      cblas_dsyrk (CblasRowMajor, CblasLower, CblasNoTrans,
		   block_size, block_size,
		   -1.0, left_in_global, N+padding_elements,
		   1.0, out, block_size);

      if(stage + 2 < x)
	create_dsyrk_task(input_data, work_data, x, blocks, block_size, stage+2, padding_elements);
    }
  }
}

void create_dpotrf_task(double* input_data, double* work_data, int x, int blocks, int block_size, int padding_elements)
{
  int self_stream = dpotrf_self_input_stream(x, blocks);
  int out_stream = dpotrf_output_stream(x, blocks);
  double self_in[block_size*block_size];
  double out[block_size*block_size];
  int N = blocks*block_size;

  create_terminal_task(work_data, x, x, N, block_size, padding_elements);

  if(x == 0) {
    #pragma omp task output(streams[out_stream] << out[block_size*block_size])
    {
      char upper = 'U';
      int n = block_size;
      int nfo;

      copy_block_from_global(input_data, out, x, x, N, block_size, padding_elements);

      dpotrf_(&upper, &n, out, &n, &nfo);
    }
  } else {
    #pragma omp task inout_reuse(streams[self_stream] >> out[block_size*block_size] >> streams[out_stream])
    {
      char upper = 'U';
      int n = block_size;
      int nfo;

      dpotrf_(&upper, &n, out, &n, &nfo);
    }
  }
}

void create_dtrsm_task(double* input_data, double* work_data, int x, int blocks, int block_size, int y, int padding_elements)
{
  int self_stream = dtrsm_self_input_stream(x, blocks, y);
  int top_stream = dtrsm_top_input_stream(x, blocks, y);
  int out_stream = dtrsm_output_stream(x, blocks, y);

  double self_in[block_size*block_size];
  double top_in[1];
  double out[block_size*block_size];

  int N = block_size*blocks;
  create_terminal_task(work_data, x, y, N, block_size, padding_elements);

  int top_in_global_x = x;
  int top_in_global_y = x;

  double* top_in_global = &work_data[top_in_global_y*block_size*(N+padding_elements)+top_in_global_x*block_size];

  if(x == 0) {
    #pragma omp task input(streams[top_stream] >> top_in[1]) \
      output(streams[out_stream] << out[block_size*block_size])
    {
      copy_block_from_global(input_data, out, x, y, N, block_size, padding_elements);
      cblas_dtrsm (CblasRowMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
		   block_size, block_size,
		   1.0, top_in_global, N+padding_elements,
		   out, block_size);
    }
  } else {
    #pragma omp task input(streams[top_stream] >> top_in[1]) \
      inout_reuse(streams[self_stream] >> out[block_size*block_size] >> streams[out_stream])
    {
      cblas_dtrsm (CblasRowMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
		   block_size, block_size,
		   1.0, top_in_global, N+padding_elements,
		   out, block_size);
    }
  }
}


void new_stream_dpotrf(double* input_data, double* work_data, int block_size, int blocks, int padding_elements)
{
  int per_block = 1;

  for(int xx = 0; xx < blocks; xx += per_block) {
    #pragma omp task
    {
      for (int x = xx; x < xx+per_block; ++x)
	{
	  if(x > 0) {
	    create_dsyrk_task(input_data, work_data, x, blocks, block_size, 0, padding_elements);

	    if(x > 1)
	      create_dsyrk_task(input_data, work_data, x, blocks, block_size, 1, padding_elements);
	  }

	  create_dpotrf_task(input_data, work_data, x, blocks, block_size, padding_elements);

	  if(x > 0) {
	    for (int y = x + 1; y < blocks; ++y) {
	      create_dgemm_task(input_data, work_data, x, blocks, block_size, y, 0, padding_elements);

	      if(x > 1)
		create_dgemm_task(input_data, work_data, x, blocks, block_size, y, 1, padding_elements);
	    }
	  }

	  for(int yy = x + 1; yy < blocks; yy += per_block)
	    {
	      for (int y = yy; y < blocks && y < yy + per_block; ++y)
		create_dtrsm_task(input_data, work_data, x, blocks, block_size, y, padding_elements);
	    }
	}
    }
  }
}


static inline void
verify (int N, double *data, double* seq_data)
{
  for(int x = 0; x < N; x++) {
    for(int y = 0; y <= x; y++) {
      if(!double_equal(data[y*N+x], seq_data[y*N+x])) {
	fprintf(stderr, "Data differs at Y = %d, X = %d: expect %.20f, but was %.20f, diff is %.20f, reldiff = %.20f\n", y, x, seq_data[y*N+x], data[y*N+x], fabs(seq_data[y*N+x] - data[y*N+x]), fabs(seq_data[y*N+x] - data[y*N+x]) / fabs(seq_data[y*N+x]));
	exit(1);
      }
    }
  }
}

int num_dgemm_streams_at_level(int l, int blocks) {
  return l*(blocks - l - 1);
}

int num_dsyrk_streams_at_level(int l, int blocks) {
  return l;
}

int num_dpotrf_streams_at_level(int l, int blocks) {
  return 1;
}

int num_dtrsm_streams_at_level(int l, int blocks) {
  return blocks - l - 1;
}

int block_input_stream(int id_x, int id_y) {
  return id_y*(id_y+1)/2 + id_x;
}


void create_dtrsm_proxy_tasks(double* data, int id_x, int id_y, int N, int block_size)
{
  int blocks = N / block_size;
  int out_stream;

  double in[block_size*block_size];
  double out[1];

  for(int stage = 0; stage <= blocks - id_x - 3; stage++) {
    out_stream = dtrsm_dgemm_proxy_output_stream(id_x, blocks, id_y, stage);

  #pragma omp task output(streams[out_stream] << out[1])
    {
    }
  }

  if(id_x != id_y) {
    out_stream = dtrsm_dsyrk_proxy_output_stream(id_x, blocks, id_y);

    #pragma omp task output(streams[out_stream] << out[1])
    {
    }
  }
}

void create_dpotrf_proxy_tasks(double* data, int id_x, int id_y, int N, int block_size)
{
  int blocks = N / block_size;
  int out_stream;

  double in[block_size*block_size];
  double out[1];

  for(int proxy_y = id_y + 1; proxy_y < blocks; proxy_y++) {
    out_stream = dpotrf_dtrsm_proxy_output_stream(id_x, blocks, proxy_y - id_x - 1);

    #pragma omp task output(streams[out_stream] << out[1])
    {
    }
  }
}

int final_input_stream(int id_x, int id_y, int blocks)
{
  if(id_x == id_y)
    return dpotrf_output_stream(id_x, blocks);

  return dtrsm_output_stream(id_x, blocks, id_y);
}

void create_df_barrier_task(void)
{
	int token[1];

	#pragma omp task output(dfbarrier_stream[0] << token[1])
	{
	}
}

void create_terminal_task(double* data, int id_x, int id_y, int N, int block_size, int padding_elements)
{
  double block_acc[block_size*block_size];
  int blocks = N / block_size;
  int input_stream;

  input_stream = final_input_stream(id_x, id_y, blocks);

  #pragma omp task input(streams[input_stream] >> block_acc[block_size*block_size])
  {
    copy_block_to_global(data, block_acc, id_x, id_y, N, block_size, padding_elements);

    if(id_x == id_y)
      create_dpotrf_proxy_tasks(data, id_x, id_y, N, block_size);
    else
      create_dtrsm_proxy_tasks(data, id_x, id_y, N, block_size);

    if(id_x == id_y && id_x == blocks-1)
      create_df_barrier_task();
  }
}

int
main(int argc, char *argv[])
{
  int option;
  int i;
  int N = 4096;

  int padding = 64;
  int padding_elements = padding / sizeof(double);

  int numiters = 10;
  int block_size = 256;

  FILE *res_file = NULL;
  FILE *in_file = NULL;

  double* seq_data;
  double* work_data;
  double * input_data;

  struct timeval start;
  struct timeval end;

  while ((option = getopt(argc, argv, "n:s:b:r:i:o:p:h")) != -1)
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
	  padding = atoi(optarg);

	  if(padding % sizeof(double)) {
	    fprintf(stderr, "Padding must be a multiple of sizeof(double) (%lu).\n",
		    sizeof(double));
	    exit(1);
	  }

	  padding_elements = padding / sizeof(double);
	  break;
	case 'h':
	  printf("Usage: %s [option]...\n\n"
		 "Options:\n"
		 "  -n <size>                    Number of colums of the square matrix, default is %d\n"
		 "  -s <power>                   Set the number of colums of the square matrix to 1 << <power>\n"
		 "  -b <block size power>        Set the block size 1 << <block size power>\n"
		 "  -r <number of iterations>    Number of repetitions of the execution\n"
		 "  -i <input file>              Read matrix data from an input file\n"
		 "  -o <output file>             Write matrix data to an output file\n"
		 "  -p <padding>                 Padding at the end of aline of the global matrix. Should be\n"
		 "                               equal to the size of one cache line, e.g. 64.\n",
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

  int size = N * N;
  int blocks = (N / block_size);

  if (posix_memalign_interleaved ((void **) &input_data, 64, size * sizeof (double) + N * padding))
    {
      printf ("Out of memory.\n");
      exit (1);
    }

  if (posix_memalign_interleaved ((void **) &work_data, 64, size * sizeof (double) + N * padding))
    {
      printf ("Out of memory.\n");
      exit (1);
    }

  // Generate random numbers or read from file
  if (in_file == NULL)
    {
      int seed[4] = {1092, 43, 77, 1};
      int sp = 1;
      dlarnv_(&sp, seed, &size, input_data);

      // Also allow saving input_data sessions
      if (res_file != NULL)
	fwrite (input_data, sizeof (double), size, res_file);
    }
  else
    {
      fread (input_data, sizeof(double), size, in_file);
    }

  // Ensure matrix is definite positive
  for(i = 0; i < N; ++i)
    input_data[i*N + i] += N;

  int num_streams = num_streams_at_level(blocks-1, blocks);
  double lstreams[num_streams] __attribute__((stream));
  streams = malloc(num_streams*sizeof(void*));
  memcpy(streams, lstreams, num_streams*sizeof(void*));

  int ldfbarrier_stream[1] __attribute__((stream));
  dfbarrier_stream = malloc(sizeof(void*));
  memcpy(dfbarrier_stream, ldfbarrier_stream, sizeof(void*));

  if(_VERIFY) {
    char upper = 'U';
    int nfo;
    seq_data = malloc(size*sizeof(double));
    memcpy(seq_data, input_data, size*sizeof(double));
    dpotrf_(&upper, &N, seq_data, &N, &nfo);
  } else {
    /* Perform at least one call to dpotrf_. The first call seems to be a lot
     * slower, probably due to lazy loading of the shared LAPACK library.
     * This call ensures that time for loading the library is not counted in
     * the parallel phase.
     */
    char upper = 'U';
    int nfo;
    int n = block_size;

    double* buffer = malloc(block_size*block_size*sizeof(double));

      for(int y = 0; y < block_size; y++)
	for(int x = 0; x < block_size; x++)
	  buffer[y*block_size+x] = input_data[y*N+x];

    dpotrf_(&upper, &n, buffer, &n, &nfo);
    free(buffer);
  }

  matrix_add_padding(input_data, N, padding_elements);

  double stream_time = 0.0;

  for(int iter = 0; iter < numiters; iter++) {
    memcpy(work_data, input_data, size*sizeof(double) + N * padding);

    gettimeofday (&start, NULL);
    openstream_start_hardware_counters();
    new_stream_dpotrf(input_data, work_data, block_size, blocks, padding_elements);

    int token[1];
    #pragma omp task input(dfbarrier_stream[0] >> token[1])
    {
    }
    #pragma omp taskwait
    openstream_pause_hardware_counters();
    gettimeofday (&end, NULL);

    stream_time += tdiff(&end, &start);

    if (_VERIFY) {
      matrix_strip_padding(work_data, N, padding_elements);
      verify (N, work_data, seq_data);
    }
  }

  printf ("%.5f \n", stream_time);

  free(input_data);
  free(work_data);

  if(_VERIFY)
    free(seq_data);

  free(streams);
  free(dfbarrier_stream);

  return 0;
}
