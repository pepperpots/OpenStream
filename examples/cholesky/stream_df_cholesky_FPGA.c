/**
 * Dataflow implementation of blocked cholesky factorization.
 * Uses clblas library for the GPU parts of the code.
 *
 * Copyright (C) 2018 Osman Seckin Simsek <osman.simsek@manchester.ac.uk>
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
#include <CL/cl.h>

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

void *streams;
void *streams2;
void *streams3;
void *dfbarrier_stream;

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
int dgemm_cnt = 0;
int syrk_cnt = 0;
int trsm_cnt = 0;
int dpotrf_cnt = 0;
int dgemm_cnt_if = 0;
int syrk_cnt_if = 0;
int trsm_cnt_if = 0;
int dpotrf_cnt_if = 0;

int final_input_stream(int id_x, int id_y, int blocks);
void create_terminal_task(double *data, int id_x, int id_y, int N, int block_size);

void copy_block_from_global(double *global_data, double *block, int id_x, int id_y, int N, int block_size)
{
  for (int y = 0; y < block_size; y++)
  {
    int global_y = id_y * block_size + y;
    memcpy(&block[y * block_size], &global_data[global_y * N + id_x * block_size], block_size * sizeof(double));
  }
}

void copy_block_to_global(double *global_data, double *block, int id_x, int id_y, int N, int block_size)
{
  for (int y = 0; y < block_size; y++)
  {
    int global_y = id_y * block_size + y;
    memcpy(&global_data[global_y * N + id_x * block_size], &block[y * block_size], block_size * sizeof(double));
  }
}

int dgemm_base(int l, int blocks)
{
  if (l == 0)
    return 0;
  else
    return num_streams_at_level(l - 1, blocks);
}

int dgemm_output_stream(int l, int blocks, int y, int stage)
{
  return dgemm_base(l, blocks) + (y - l - 1) * (l) + stage;
}

int dgemm_self_input_stream(int l, int blocks, int y, int stage)
{
  return dgemm_output_stream(l, blocks, y, stage - 1);
}

int dgemm_upper_input_stream(int l, int blocks, int y, int stage)
{
  return dtrsm_dgemm_proxy_output_stream(stage, blocks, l, y - l - 1);
}

int dgemm_lower_input_stream(int l, int blocks, int y, int stage)
{
  return dtrsm_dgemm_proxy_output_stream(stage, blocks, y,
                                         (blocks - 2 - stage) - (y - stage - 1) + (l - stage) - 1);
}

int dsyrk_base(int l, int blocks)
{
  return dgemm_base(l, blocks) + num_dgemm_streams_at_level(l, blocks);
}

int dsyrk_left_input_stream(int l, int blocks, int stage)
{
  return dtrsm_dsyrk_proxy_output_stream(stage, blocks, l);
}

int dsyrk_self_input_stream(int l, int blocks, int stage)
{
  return dsyrk_output_stream(l, blocks, stage - 1);
}

int dsyrk_output_stream(int l, int blocks, int stage)
{
  return dsyrk_base(l, blocks) + stage;
}

int dpotrf_base(int l, int blocks)
{
  return dsyrk_base(l, blocks) + num_dsyrk_streams_at_level(l, blocks);
}

int dpotrf_self_input_stream(int l, int blocks)
{
  return dsyrk_output_stream(l, blocks, l - 1);
}

int dpotrf_output_stream(int l, int blocks)
{
  return dpotrf_base(l, blocks);
}

int dtrsm_base(int l, int blocks)
{
  return dpotrf_base(l, blocks) + 1;
}

int dtrsm_self_input_stream(int l, int blocks, int y)
{
  return dgemm_output_stream(l, blocks, y, l - 1);
}

int dtrsm_top_input_stream(int l, int blocks, int y)
{
  return dpotrf_dtrsm_proxy_output_stream(l, blocks, y - l - 1);
}

int dtrsm_output_stream(int l, int blocks, int y)
{
  return dtrsm_base(l, blocks) + y - l - 1;
}

int num_dpotrf_dtrsm_proxy_streams_at_level(int l, int blocks)
{
  return blocks - l - 1;
}

int dpotrf_dtrsm_proxy_base(int l, int blocks)
{
  return dtrsm_base(l, blocks) + num_dtrsm_streams_at_level(l, blocks);
}

int dpotrf_dtrsm_proxy_output_stream(int l, int blocks, int stage)
{
  return dpotrf_dtrsm_proxy_base(l, blocks) + stage;
}

int num_dtrsm_dgemm_proxy_streams_at_level(int l, int blocks)
{
  if (l == blocks - 1)
    return 0;

  return (blocks - l - 2) * (blocks - l - 1);
}

int dtrsm_dgemm_proxy_base(int l, int blocks)
{
  return dpotrf_dtrsm_proxy_base(l, blocks) + num_dpotrf_dtrsm_proxy_streams_at_level(l, blocks);
}

int dtrsm_dgemm_proxy_output_stream(int l, int blocks, int y, int stage)
{
  return dtrsm_dgemm_proxy_base(l, blocks) +
         (y - l - 1) * (blocks - l - 2) + stage;
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
  return blocks - l - 1;
}

int num_streams_at_level(int l, int blocks)
{
  int nstreams_upper;

  if (l > 0)
    nstreams_upper = num_streams_at_level(l - 1, blocks);
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

void create_dgemm_task(double *input_data, int x, int blocks, int block_size, int y, int stage)
{
  int upper_stream = dgemm_upper_input_stream(x, blocks, y, stage);
  int lower_stream = dgemm_lower_input_stream(x, blocks, y, stage);
  int self_stream = dgemm_self_input_stream(x, blocks, y, stage);
  int out_stream = dgemm_output_stream(x, blocks, y, stage);

  double upper_in[block_size * block_size];
  double lower_in[block_size * block_size];
  double self_in[block_size * block_size];
  double out[block_size * block_size];
  double self_out[block_size * block_size];

  int N = block_size * blocks;

  if (stage == 0)
  {
    dgemm_cnt_if++;
#pragma omp task output(streams3[y - 1] << self_out[block_size * block_size])
    {
      copy_block_from_global(input_data, self_out, x, y, N, block_size);
    }

#pragma omp task input(streams[upper_stream] >> upper_in[block_size * block_size]) \
                 input(streams[lower_stream] >> lower_in[block_size * block_size]) \
                 input(streams3[y - 1] >> self_in[block_size * block_size]) \
                 output(streams[out_stream] << out[block_size * block_size]) \
                 accel_name(Partial_dgemm) \
                 args(upper_in, lower_in, self_in, out)
    {
      // copy_block_from_global(input_data, out, x, y, N, block_size);
      memcpy(out, self_in, block_size * block_size * sizeof(double));

      cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasTrans,
                  block_size, block_size, block_size,
                  -1.0, upper_in, block_size,
                  lower_in, block_size,
                  1.0, out, block_size);
    }
  }
  else
  {
    dgemm_cnt++;
#pragma omp task input(streams[upper_stream] >> upper_in[block_size * block_size], \
                       streams[lower_stream] >> lower_in[block_size * block_size], \
                       streams[self_stream] >> self_in[block_size * block_size])   \
                       output(streams[out_stream] << out[block_size * block_size]) \
                       accel_name(Partial_dgemm) \
                       args(upper_in, lower_in, self_in, out)
    {
      memcpy(out, self_in, block_size * block_size * sizeof(double));

      cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasTrans,
                  block_size, block_size, block_size,
                  -1.0, upper_in, block_size,
                  lower_in, block_size,
                  1.0, out, block_size);

      // if (stage + 2 < x)
      //   create_dgemm_task(input_data, x, blocks, block_size, y, stage + 2);
    }
  }

  if (stage + 2 < x)
    create_dgemm_task(input_data, x, blocks, block_size, y, stage + 2);
}

void create_dsyrk_task(double *input_data, int x, int blocks, int block_size, int stage)
{
  int self_stream = dsyrk_self_input_stream(x, blocks, stage);
  int left_stream = dsyrk_left_input_stream(x, blocks, stage);
  int out_stream = dsyrk_output_stream(x, blocks, stage);

  double self_in[block_size * block_size];
  double left_in[block_size * block_size];
  double out[block_size * block_size];
  double self_out[block_size * block_size];

  int N = block_size * blocks;

  if (stage == 0)
  {
    syrk_cnt_if++;
#pragma omp task output(streams[self_stream] << self_out[block_size * block_size])
    {
      copy_block_from_global(input_data, self_out, x, x, N, block_size);
    }

#pragma omp task input(streams[left_stream] >> left_in[block_size * block_size]) \
                 input(streams[self_stream] >> self_in[block_size * block_size]) \
                 output(streams[out_stream] << out[block_size * block_size]) \
                 accel_name(Partial_dsyrk) \
                 args(left_in, self_in, out)
    {
      // copy_block_from_global(input_data, out, x, x, N, block_size);
      memcpy(out, self_in, block_size * block_size * sizeof(double));
      cblas_dsyrk(CblasRowMajor, CblasLower, CblasNoTrans,
                  block_size, block_size,
                  -1.0, left_in, block_size,
                  1.0, out, block_size);

      // if (stage + 2 < x)
      //   create_dsyrk_task(input_data, x, blocks, block_size, stage + 2);
    }
  }
  else
  {
    syrk_cnt++;
#pragma omp task input(streams[left_stream] >> left_in[block_size * block_size], \
                       streams[self_stream] >> self_in[block_size * block_size]) \
                       output(streams[out_stream] << out[block_size * block_size]) \
                       accel_name(Partial_dsyrk) \
                       args(left_in, self_in, out)
    {
      memcpy(out, self_in, block_size * block_size * sizeof(double));

      cblas_dsyrk(CblasRowMajor, CblasLower, CblasNoTrans,
                  block_size, block_size,
                  -1.0, left_in, block_size,
                  1.0, out, block_size);
    }
  }

  if (stage + 2 < x)
    create_dsyrk_task(input_data, x, blocks, block_size, stage + 2);
}

void create_dpotrf_task(double *input_data, double *work_data, int x, int blocks, int block_size)
{
  int self_stream = dpotrf_self_input_stream(x, blocks);
  int out_stream = dpotrf_output_stream(x, blocks);
  double self_in[block_size * block_size];
  double out[block_size * block_size];
  int N = blocks * block_size;

  create_terminal_task(work_data, x, x, N, block_size);

  if (x == 0)
  {
    dpotrf_cnt_if++;
#pragma omp task output(streams[out_stream] << out[block_size * block_size])
    {
      char upper = 'U';
      int n = block_size;
      int nfo;

      copy_block_from_global(input_data, out, x, x, N, block_size);

      dpotrf_(&upper, &n, out, &n, &nfo);
    }
  }
  else
  {
    dpotrf_cnt++;
#pragma omp task input(streams[self_stream] >> self_in[block_size * block_size]) \
    output(streams[out_stream] << out[block_size * block_size])
    {
      char upper = 'U';
      int n = block_size;
      int nfo;

      memcpy(out, self_in, block_size * block_size * sizeof(double));

      dpotrf_(&upper, &n, out, &n, &nfo);
    }
  }
}

void create_dtrsm_task(double *input_data, double *work_data, int x, int blocks, int block_size, int y)
{
  int self_stream = dtrsm_self_input_stream(x, blocks, y);
  int top_stream = dtrsm_top_input_stream(x, blocks, y);
  int out_stream = dtrsm_output_stream(x, blocks, y);

  double self_in[block_size * block_size];
  double top_in[block_size * block_size];
  double out[block_size * block_size];
  double self_out[block_size * block_size];

  int N = block_size * blocks;
  create_terminal_task(work_data, x, y, N, block_size);

  if (x == 0)
  {
    trsm_cnt_if++;
#pragma omp task output(streams2[y - 1] << self_out[block_size * block_size])
    {
      copy_block_from_global(input_data, self_out, x, y, N, block_size);
    }
#pragma omp task input(streams[top_stream] >> top_in[block_size * block_size]) \
                 input(streams2[y - 1] >> self_in[block_size * block_size]) \
                 output(streams[out_stream] << out[block_size * block_size]) \
                 accel_name(Partial_dtrsm) \
                 args(top_in, self_in, out)
    {
      // copy_block_from_global(input_data, out, x, y, N, block_size);
      memcpy(out, self_in, block_size * block_size * sizeof(double));

      cblas_dtrsm(CblasRowMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
                  block_size, block_size,
                  1.0, top_in, block_size,
                  out, block_size);
    }
  }
  else
  {
    trsm_cnt++;
#pragma omp task input(streams[top_stream] >> top_in[block_size * block_size],   \
                       streams[self_stream] >> self_in[block_size * block_size]) \
                       output(streams[out_stream] << out[block_size * block_size]) \
                       accel_name(Partial_dtrsm) \
                       args(top_in, self_in, out)
    {
      memcpy(out, self_in, block_size * block_size * sizeof(double));
      cblas_dtrsm(CblasRowMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
                  block_size, block_size,
                  1.0, top_in, block_size,
                  out, block_size);
    }
  }
}

void new_stream_dpotrf(double *input_data, double *work_data, int block_size, int blocks)
{
  int per_block = 1;

  for (int xx = 0; xx < blocks; xx += per_block)
  {
#pragma omp task
    {
      for (int x = xx; x < xx + per_block; ++x)
      {
        if (x > 0)
        {
          create_dsyrk_task(input_data, x, blocks, block_size, 0);

          if (x > 1)
          {
            create_dsyrk_task(input_data, x, blocks, block_size, 1);
          }
        }

        create_dpotrf_task(input_data, work_data, x, blocks, block_size);

        if (x > 0)
        {
          for (int y = x + 1; y < blocks; ++y)
          {
            create_dgemm_task(input_data, x, blocks, block_size, y, 0);

            if (x > 1)
            {
              create_dgemm_task(input_data, x, blocks, block_size, y, 1);
            }
          }
        }

        for (int yy = x + 1; yy < blocks; yy += per_block)
        {
          for (int y = yy; y < blocks && y < yy + per_block; ++y)
          {
            create_dtrsm_task(input_data, work_data, x, blocks, block_size, y);
          }
        }
      }
    }
  }
}

static inline void
verify(int N, double *data, double *seq_data)
{
  for (int x = 0; x < N; x++)
  {
    for (int y = 0; y <= x; y++)
    {
      if (!double_equal(data[y * N + x], seq_data[y * N + x]))
      {
        fprintf(stderr, "Data differs at Y = %d, X = %d: expect %.20f, but was %.20f, diff is %.20f, reldiff = %.20f\n", y, x, seq_data[y * N + x], data[y * N + x], fabs(seq_data[y * N + x] - data[y * N + x]), fabs(seq_data[y * N + x] - data[y * N + x]) / fabs(seq_data[y * N + x]));
        exit(1);
      }
    }
  }
  fprintf(stdout, "RESULTS CORRECT!\n");
}

int num_dgemm_streams_at_level(int l, int blocks)
{
  return l * (blocks - l - 1);
}

int num_dsyrk_streams_at_level(int l, int blocks)
{
  return l;
}

int num_dpotrf_streams_at_level(int l, int blocks)
{
  return 1;
}

int num_dtrsm_streams_at_level(int l, int blocks)
{
  return blocks - l - 1;
}

int block_input_stream(int id_x, int id_y)
{
  return id_y * (id_y + 1) / 2 + id_x;
}

void create_dtrsm_proxy_tasks(double *data, int id_x, int id_y, int N, int block_size)
{
  int blocks = N / block_size;
  int out_stream;

  double in[block_size * block_size];
  double out[block_size * block_size];

  for (int stage = 0; stage <= blocks - id_x - 3; stage++)
  {
    out_stream = dtrsm_dgemm_proxy_output_stream(id_x, blocks, id_y, stage);

#pragma omp task output(streams[out_stream] << out[block_size * block_size])
    {
      copy_block_from_global(data, out, id_x, id_y, N, block_size);
    }
  }

  if (id_x != id_y)
  {
    out_stream = dtrsm_dsyrk_proxy_output_stream(id_x, blocks, id_y);

#pragma omp task output(streams[out_stream] << out[block_size * block_size])
    {
      copy_block_from_global(data, out, id_x, id_y, N, block_size);
    }
  }
}

void create_dpotrf_proxy_tasks(double *data, int id_x, int id_y, int N, int block_size)
{
  int blocks = N / block_size;
  int out_stream;

  double in[block_size * block_size];
  double out[block_size * block_size];

  for (int proxy_y = id_y + 1; proxy_y < blocks; proxy_y++)
  {
    out_stream = dpotrf_dtrsm_proxy_output_stream(id_x, blocks, proxy_y - id_x - 1);

#pragma omp task output(streams[out_stream] << out[block_size * block_size])
    {
      copy_block_from_global(data, out, id_x, id_y, N, block_size);
    }
  }
}

int final_input_stream(int id_x, int id_y, int blocks)
{
  if (id_x == id_y)
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

void create_terminal_task(double *data, int id_x, int id_y, int N, int block_size)
{
  double block_acc[block_size * block_size];
  int blocks = N / block_size;
  int input_stream;

  input_stream = final_input_stream(id_x, id_y, blocks);

#pragma omp task input(streams[input_stream] >> block_acc[block_size * block_size])
  {
    copy_block_to_global(data, block_acc, id_x, id_y, N, block_size);

    if (id_x == id_y)
      create_dpotrf_proxy_tasks(data, id_x, id_y, N, block_size);
    else
      create_dtrsm_proxy_tasks(data, id_x, id_y, N, block_size);

    if (id_x == id_y && id_x == blocks - 1)
      create_df_barrier_task();
  }
}

int main(int argc, char *argv[])
{
  int option;
  int i;
  int N = 4096;

  int numiters = 10;
  int block_size = 256;

  FILE *res_file = NULL;
  FILE *in_file = NULL;

  double *seq_data;
  double *work_data;
  double *input_data;

  struct timeval start;
  struct timeval end;

  while ((option = getopt(argc, argv, "n:s:b:r:i:o:h")) != -1)
  {
    switch (option)
    {
    case 'n':
      N = atoi(optarg);
      break;
    case 's':
      N = 1 << atoi(optarg);
      break;
    case 'b':
      block_size = 1 << atoi(optarg);
      break;
    case 'r':
      numiters = atoi(optarg);
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

  if (optind != argc)
  {
    fprintf(stderr, "Too many arguments. Run %s -h for usage.\n", argv[0]);
    exit(1);
  }

  int size = N * N;
  int blocks = (N / block_size);

  if (posix_memalign_interleaved((void **)&input_data, 64, size * sizeof(double)))
  {
    printf("Out of memory.\n");
    exit(1);
  }

  if (posix_memalign_interleaved((void **)&work_data, 64, size * sizeof(double)))
  {
    printf("Out of memory.\n");
    exit(1);
  }

  // Generate random numbers or read from file
  if (in_file == NULL)
  {
    int seed[4] = {1092, 43, 77, 1};
    int sp = 1;
    dlarnv_(&sp, seed, &size, input_data);

    // Also allow saving input_data sessions
    if (res_file != NULL)
      fwrite(input_data, sizeof(double), size, res_file);
  }
  else
  {
    fread(input_data, sizeof(double), size, in_file);
  }

  // Ensure matrix is definite positive
  for (i = 0; i < N; ++i)
    input_data[i * N + i] += N;

  int num_streams = num_streams_at_level(blocks - 1, blocks);
  double lstreams[num_streams] __attribute__((stream));
  streams = malloc(num_streams * sizeof(void *));
  memcpy(streams, lstreams, num_streams * sizeof(void *));

  // FIXME: magic numbers
  int dtrsm_num_streams = 64;
  int dgemm_num_streams = 2048;

  double lstreams2[dtrsm_num_streams] __attribute__((stream));
  streams2 = malloc(dtrsm_num_streams * sizeof(void *));
  memcpy(streams2, lstreams2, dtrsm_num_streams * sizeof(void *));

  double lstreams3[dgemm_num_streams] __attribute__((stream));
  streams3 = malloc(dgemm_num_streams * sizeof(void *));
  memcpy(streams3, lstreams3, dgemm_num_streams * sizeof(void *));

  int ldfbarrier_stream[1] __attribute__((stream));
  dfbarrier_stream = malloc(sizeof(void *));
  memcpy(dfbarrier_stream, ldfbarrier_stream, sizeof(void *));

  if (_VERIFY)
  {
    char upper = 'U';
    int nfo;
    seq_data = malloc(size * sizeof(double));
    memcpy(seq_data, input_data, size * sizeof(double));
    dpotrf_(&upper, &N, seq_data, &N, &nfo);
  }
  else
  {
    /* Perform at least one call to dpotrf_. The first call seems to be a lot
     * slower, probably due to lazy loading of the shared LAPACK library.
     * This call ensures that time for loading the library is not counted in
     * the parallel phase.
     */
    char upper = 'U';
    int nfo;
    int n = block_size;

    double *buffer = malloc(block_size * block_size * sizeof(double));

    for (int y = 0; y < block_size; y++)
      for (int x = 0; x < block_size; x++)
        buffer[y * block_size + x] = input_data[y * N + x];

    dpotrf_(&upper, &n, buffer, &n, &nfo);
    free(buffer);
  }

  double stream_time = 0.0;

  for (int iter = 0; iter < numiters; iter++)
  {
    memcpy(work_data, input_data, size * sizeof(double));

    gettimeofday(&start, NULL);
    openstream_start_hardware_counters();
    new_stream_dpotrf(input_data, work_data, block_size, blocks);

    int token[1];
#pragma omp task input(dfbarrier_stream[0] >> token[1])
    {
    }
#pragma omp taskwait
    openstream_pause_hardware_counters();
    gettimeofday(&end, NULL);

    stream_time += tdiff(&end, &start);

    if (_VERIFY)
      verify(N, work_data, seq_data);
  }

  printf("%.5f \n", stream_time);
  free(input_data);
  free(work_data);

  if (_VERIFY)
    free(seq_data);

  free(streams);
  free(dfbarrier_stream);
  printf("dgemm_cnt     = %d\n", dgemm_cnt);
  printf("dgemm_cnt_if  = %d\n", dgemm_cnt_if);
  printf("syrk_cnt      = %d\n", syrk_cnt);
  printf("syrk_cnt_if   = %d\n", syrk_cnt_if);
  printf("trsm_cnt      = %d\n", trsm_cnt);
  printf("trsm_cnt_if   = %d\n", trsm_cnt_if);
  printf("dpotrf_cnt    = %d\n", dpotrf_cnt);
  printf("dpotrf_cnt_if = %d\n", dpotrf_cnt_if);

  return 0;
}
