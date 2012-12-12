#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include "../common/common.h"

#define _WITH_SPEEDUPS 0
#define _CHECK_STREAM 0
#define _CHECK_SEQUENTIAL 0
#define _WITH_OUTPUT 0

#include <unistd.h>

static inline void *
allocate_block (int block_size)
{
  void *res;

  posix_memalign ((void**)&res, 64, block_size * block_size * sizeof(double));
  assert (res != NULL);
  memset (res, 0, block_size * block_size * sizeof(double));

  return res;
}

static int full_blocks = 0;

static inline void
generate_block_sparse_matrix (int num_blocks, int block_size,
			      void * (*mat)[num_blocks][num_blocks])
{
  int i, j, k, l;

  for (i = 0; i < num_blocks; ++i)
    for (j = 0; j < num_blocks; ++j)
      {
	if (i == j || i == j - 1 || i == j + 1
	    || ((i % 13 == 0) && (j % 5 != 0) && (j % 3 == 0))
	    || (((i + j) % 17 == 0) && (j % 2 != 0)))
	  {
	    (*mat)[i][j] = allocate_block (block_size);
	    ++full_blocks;
	  }
      }

  for (i = 0; i < num_blocks; ++i)
    for (j = 0; j < num_blocks; ++j)
      {
	if ((*mat)[i][j] == NULL)
	  continue;
	for (k = 0; k < block_size; ++k)
	  for (l = 0; l < block_size; ++l)
	    {
	      double (*block)[block_size][block_size] = (*mat)[i][j];

	      (*block)[k][l] = 0.000077 * (i % 17 + j % 11 + (k * l) % 7);

	      if (i == j)
		{
		  if (k == l)
		    (*block)[k][l] = -2 * num_blocks * num_blocks * block_size ;
		  if (k == l + 1 || k == l - 1)
		    (*block)[k][l] = num_blocks * block_size * block_size ;
		}
	    }
      }
}

static inline void
split_block (int block_size,
	     double (*block_a)[block_size][block_size],
	     double (*block_l)[block_size][block_size],
	     double (*block_u)[block_size][block_size])
{
  int i, j;

  for (i = 0; i < block_size; ++i)
    for (j = 0; j < block_size; ++j)
      {
        if (i == j)
	  {
	    (*block_l)[i][j] = 1.0;
	    (*block_u)[i][j] = (*block_a)[i][j];
	  }
        else if (i > j)
	  {
	    (*block_l)[i][j] = (*block_a)[i][j];
	    (*block_u)[i][j] = 0.0;
	  }
	else
	  {
	    (*block_l)[i][j] = 0.0;
	    (*block_u)[i][j] = (*block_a)[i][j];
	  }
      }
}

static inline void
copy_block (int block_size,
	    double (*block_a)[block_size][block_size],
	    double (*block_b)[block_size][block_size])
{
  int i, j;

  for (i = 0; i < block_size; ++i)
    for (j = 0; j < block_size; ++j)
      (*block_a)[i][j] = (*block_b)[i][j];
}

static inline void
zero_block (int block_size,
	    double (*block)[block_size][block_size])
{
  memset (&(*block)[0][0], 0, block_size * block_size * sizeof (double));
}


static inline void
split_matrix (int num_blocks, int block_size,
	      void * (*mat_lu)[num_blocks][num_blocks],
	      void * (*mat_l)[num_blocks][num_blocks],
	      void * (*mat_u)[num_blocks][num_blocks])
{
  double (*block)[block_size][block_size];
  int i, j;

  for (i = 0; i < num_blocks; ++i)
    for (j = 0; j < num_blocks; ++j)
      {
	if (i == j)
	  {
	    (*mat_l)[i][i] = allocate_block (block_size);
	    (*mat_u)[i][i] = allocate_block (block_size);
	    split_block (block_size, (*mat_lu)[i][i], (*mat_l)[i][i], (*mat_u)[i][i]);
	  }
	else
	  {
	    if ((*mat_lu)[i][j] != NULL)
	      {
		block = allocate_block (block_size);
		copy_block (block_size, block, (*mat_lu)[i][j]);
	      }
	    else
	      block = NULL;
	    if (i > j)
	      {
		(*mat_l)[i][j] = block;
		(*mat_u)[i][j] = NULL;
	      }
	    else
	      {
		(*mat_l)[i][j] = NULL;
		(*mat_u)[i][j] = block;
	      }
	  }
      }
}

static inline void
duplicate (int num_blocks, int block_size,
	   void * (*mat_a)[num_blocks][num_blocks],
	   void * (*mat_b)[num_blocks][num_blocks])
{
  double (*block)[block_size][block_size];
  int i, j;

  for (i = 0; i < num_blocks; ++i)
    for (j = 0; j < num_blocks; ++j)
      if ((*mat_b)[i][j] != NULL)
	{
	  block = allocate_block (block_size);
	  copy_block (block_size, block, (*mat_b)[i][j]);
	  (*mat_a)[i][j] = block;
	}
      else
	(*mat_a)[i][j]=NULL;
}

static inline void
clear_matrix (int num_blocks, int block_size,
	      void * (*mat)[num_blocks][num_blocks])
{
  int i, j;

  for (i = 0; i < num_blocks; ++i)
    for (j = 0; j < num_blocks; ++j)
      if ((*mat)[i][j] != NULL)
	zero_block (block_size, (*mat)[i][j]);
}


/* Sequential multiplication.  */
static inline void
block_multiply_add (int block_size,
		    double (*block_a)[block_size][block_size],
		    double (*block_b)[block_size][block_size],
		    double (*block_c)[block_size][block_size])
{
  int i, j, k;

  for (i = 0; i < block_size; ++i)
    for (j = 0; j < block_size; ++j)
      for (k = 0; k < block_size; ++k)
	(*block_c)[i][j] += (*block_a)[i][k] * (*block_b)[k][j];
}

static inline void
sparse_matmult (int num_blocks, int block_size,
		void * (*mat_a)[num_blocks][num_blocks],
		void * (*mat_b)[num_blocks][num_blocks],
		void * (*mat_c)[num_blocks][num_blocks])
{
  int i, j, k;

  for (i = 0; i < num_blocks; ++i)
    for (j = 0; j < num_blocks; ++j)
      for (k = 0; k < num_blocks; ++k)
	if (((*mat_a)[i][k] != NULL) && ((*mat_b)[k][j] != NULL))
	  {
	    if ((*mat_c)[i][j] == NULL)
	      (*mat_c)[i][j] = allocate_block (block_size);
	    block_multiply_add (block_size, (*mat_a)[i][k], (*mat_b)[k][j], (*mat_c)[i][j]);
	  }
}

static inline double
block_diff (int block_size,
	    double (*block_a)[block_size][block_size],
	    double (*block_b)[block_size][block_size])
{
  double tmp, res = 0.0;
  int i, j;

  for (i = 0; i < block_size; ++i)
    for (j = 0; j < block_size; ++j)
      {
	double va = (block_a != NULL) ? (*block_a)[i][j] : 0.0;
	double vb = (block_b != NULL) ? (*block_b)[i][j] : 0.0;
	tmp = va - vb;
	res += tmp * tmp;
      }
  return res;
}

static inline void
matrix_diff (int num_blocks, int block_size,
	     void * (*mat_a)[num_blocks][num_blocks],
	     void * (*mat_b)[num_blocks][num_blocks])
{
  double *error = (double *) calloc (num_blocks*num_blocks, sizeof (double));
  int i, j;

  for (i = 0; i < num_blocks; ++i)
    for (j = 0; j < num_blocks; ++j)
      if ((*mat_a)[i][j] != NULL || (*mat_b)[i][j] != NULL)
	error[i*num_blocks + j] = block_diff (block_size, (*mat_a)[i][j], (*mat_b)[i][j]);
      else
	error[i*num_blocks + j] = 0.0;

  for (i = 0; i < num_blocks; ++i)
    for (j = 0; j < num_blocks; ++j)
      if (double_equal (error[i*num_blocks + j], 0.0) == false)
	{
	  printf ("ERROR: Matrices differ at block (%d, %d). Error is %lf\n", i, j, error[i*num_blocks + j]);
	  exit (1);
	}
  free (error);
}


/* ----------------------------------------------------- */
static inline void
lu_block (int block_size, double (*block)[block_size][block_size])
{
  int i, j, k;

  for (k = 0; k < block_size; ++k)
    for (i = k + 1; i < block_size; ++i)
      {
	(*block)[i][k] = (*block)[i][k] / (*block)[k][k];
	for (j = k + 1; j < block_size; ++j)
	  (*block)[i][j] -= (*block)[i][k] * (*block)[k][j];
      }
}

static inline void
bdiv (int block_size,
      double (*block_d)[block_size][block_size],
      double (*block_r)[block_size][block_size])
{
  int i, j, k;

  for (i = 0; i < block_size; ++i)
    for (k = 0; k < block_size; ++k)
      {
	(*block_r)[i][k] = (*block_r)[i][k] / (*block_d)[k][k];
	for (j = k + 1; j < block_size; ++j)
	  (*block_r)[i][j] -= (*block_r)[i][k] * (*block_d)[k][j];
      }
}

static inline void
bmod (int block_size,
      double (*block_r)[block_size][block_size],
      double (*block_c)[block_size][block_size],
      double (*block_i)[block_size][block_size])
{
  int i, j, k;

  for (i = 0; i < block_size; ++i)
    for (j = 0; j < block_size; ++j)
      for (k = 0; k < block_size; ++k)
	(*block_i)[i][j] -= (*block_r)[i][k] * (*block_c)[k][j];

}

static inline void
fwd (int block_size,
     double (*block_d)[block_size][block_size],
     double (*block_c)[block_size][block_size])
{
  int i, j, k;

  for (j = 0; j < block_size; ++j)
    for (k = 0; k < block_size; ++k)
      for (i = k + 1; i < block_size; i++)
	(*block_c)[i][j] -= (*block_d)[i][k] * (*block_c)[k][j];
}


/* Main work function.  */
static inline void
stream_factorize (int num_blocks, int block_size,
		  void * (*data)[num_blocks][num_blocks],
		  int Rstreams[num_blocks*num_blocks] __attribute__((stream)),
		  int Wstreams[num_blocks*num_blocks] __attribute__((stream)),
		  int counters[num_blocks*num_blocks])
{
  int i, j, k;

  for (k=0; k<num_blocks; k++)
    {
      // TASK 1
      {
	int readers = counters[k*num_blocks + k];
	int win, wout;

	if (readers == 0)
	  {
#pragma omp task input (Wstreams[k*num_blocks + k] >> win) output (Wstreams[k*num_blocks + k] << wout)
	    {
	      lu_block (block_size, (*data)[k][k]);
	    }
	  }
	else
	  {
	    int Rwin[readers];
	    counters[k*num_blocks + k] = 0;

#pragma omp task input (Wstreams[k*num_blocks + k] >> win) output (Wstreams[k*num_blocks + k] << wout) \
  input (Rstreams[k*num_blocks + k] >> Rwin[readers])
	    {
	      lu_block (block_size, (*data)[k][k]);
	    }
	  }
      }

      // TASK 2
      for (j=k+1; j<num_blocks; j++)
	if ((*data)[k][j] != NULL)
	  {
	    int readers = counters[k*num_blocks + j];
	    int win, wout;
	    int win2, wout2;

	    counters[k*num_blocks + k] += 1;

	    if (readers == 0)
	      {
#pragma omp task peek (Wstreams[k*num_blocks + k] >> win) output (Rstreams[k*num_blocks + k] << wout) \
  input (Wstreams[k*num_blocks + j] >> win2) output (Wstreams[k*num_blocks + j] << wout2)
		{
		  fwd (block_size, (*data)[k][k], (*data)[k][j]);
		}
	      }
	    else
	      {
		int Rwin[readers];
		counters[k*num_blocks + j] = 0;

#pragma omp task peek (Wstreams[k*num_blocks + k] >> win) output (Rstreams[k*num_blocks + k] << wout) \
  input (Wstreams[k*num_blocks + j] >> win2) output (Wstreams[k*num_blocks + j] << wout2) \
  input (Rstreams[k*num_blocks + j] >> Rwin[readers])
		{
		  fwd (block_size, (*data)[k][k], (*data)[k][j]);
		}
	      }
	  }

      // TASK 3
      for (i = k + 1; i < num_blocks; ++i)
	if ((*data)[i][k] != NULL)
	  {
	    int readers = counters[i*num_blocks + k];
	    int win, wout;
	    int win2, wout2;

	    counters[k*num_blocks + k] += 1;

	    if (readers == 0)
	      {
#pragma omp task peek (Wstreams[k*num_blocks + k] >> win) output (Rstreams[k*num_blocks + k] << wout) \
  input (Wstreams[i*num_blocks + k] >> win2) output (Wstreams[i*num_blocks + k] << wout2)
		{
		  bdiv (block_size, (*data)[k][k], (*data)[i][k]);
		}
	      }
	    else
	      {
		int Rwin[readers];
		counters[i*num_blocks + k] = 0;

#pragma omp task peek (Wstreams[k*num_blocks + k] >> win) output (Rstreams[k*num_blocks + k] << wout) \
  input (Wstreams[i*num_blocks + k] >> win2) output (Wstreams[i*num_blocks + k] << wout2) \
  input (Rstreams[i*num_blocks + k] >> Rwin[readers])
		{
		  bdiv (block_size, (*data)[k][k], (*data)[i][k]);
		}
	      }
	  }

      // TASK 4
      for (i = k + 1; i < num_blocks; ++i)
	{
	if ((*data)[i][k] != NULL)
	  {
	  for (j = k + 1; j < num_blocks; ++j)
	    {
	      if ((*data)[k][j] != NULL)
		{
		  if ((*data)[i][j]==NULL)
		    (*data)[i][j] = allocate_block (block_size);

		  int readers = counters[i*num_blocks + j];
		  int wina, winb, wout;
		  int win2, wouta2, woutb2;

		  counters[i*num_blocks + k] += 1;
		  counters[k*num_blocks + j] += 1;

		  if (readers == 0)
		    {
#pragma omp task peek (Wstreams[i*num_blocks + k] >> wina, Wstreams[k*num_blocks + j] >> winb) \
  output (Rstreams[i*num_blocks + k] << wouta2, Rstreams[k*num_blocks + j] << woutb2) \
  output (Wstreams[i*num_blocks + j] << wout) input (Wstreams[i*num_blocks + j] >> win2)
		      {
			bmod (block_size, (*data)[i][k], (*data)[k][j], (*data)[i][j]);
		      }
		    }
		  else
		    {
		      int Rwin[readers];
		      counters[i*num_blocks + j] = 0;

#pragma omp task peek (Wstreams[i*num_blocks + k] >> wina, Wstreams[k*num_blocks + j] >> winb) \
  output (Rstreams[i*num_blocks + k] << wouta2, Rstreams[k*num_blocks + j] << woutb2) \
  output (Wstreams[i*num_blocks + j] << wout) input (Wstreams[i*num_blocks + j] >> win2) \
  input (Rstreams[i*num_blocks + j] >> Rwin[readers])
		      {
			bmod (block_size, (*data)[i][k], (*data)[k][j], (*data)[i][j]);
		      }
		    }
		}
	    }
	}
      }
    }
}

static inline void
sequential_factorize (int num_blocks, int block_size,
		      void * (*data)[num_blocks][num_blocks])
{
  int i, j, k;
  for (k=0; k<num_blocks; k++)
    {
      lu_block (block_size, (*data)[k][k]);

      for (j = k + 1; j < num_blocks; ++j)
	if ((*data)[k][j] != NULL)
	  fwd (block_size, (*data)[k][k], (*data)[k][j]);

      for (i = k + 1; i < num_blocks; ++i)
	if ((*data)[i][k] != NULL)
	  bdiv (block_size, (*data)[k][k], (*data)[i][k]);

      for (i = k + 1; i < num_blocks; ++i)
	if ((*data)[i][k] != NULL)
	  for (j = k + 1; j < num_blocks; ++j)
	    if ((*data)[k][j] != NULL)
	      {
		if ((*data)[i][j]==NULL)
		  (*data)[i][j] = allocate_block (block_size);

		bmod (block_size, (*data)[i][k], (*data)[k][j], (*data)[i][j]);
	      }
    }
}


int
main (int argc, char* argv[])
{
  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));

  int option;
  int i;
  int N = 64;

  int block_size = 8;

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
	case 'o':
	  res_file = fopen(optarg, "w");
	  break;
	case 'h':
	  printf ("Usage: %s [option]...\n\n"
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

  int num_blocks = N / block_size;
  int all_blocks = num_blocks * num_blocks;

  int Rstreams[all_blocks] __attribute__((stream));
  int Wstreams[all_blocks] __attribute__((stream));
  int final_view[all_blocks][1];
  int counters[all_blocks];

  void * data;
  void * bckp_data;
  data = (void * (*)[num_blocks][num_blocks]) calloc (all_blocks, sizeof (void *));
  bckp_data = (void * (*)[num_blocks][num_blocks]) calloc (all_blocks, sizeof (void *));

  /* Initialize streams and counters.  */
  for (i = 0; i < num_blocks*num_blocks; ++i)
    {
      int x;
      counters[i] = 0;

#pragma omp task output (Wstreams[i] << x)
      x = 0;
    }

  if (res_file == NULL)
    res_file = fopen("stream_sparse-lu.out", "w");

  /* Initialize the matrix to factorize and make a copy for testing
     correctness.  */
  generate_block_sparse_matrix (num_blocks, block_size, data);
  duplicate (num_blocks, block_size, bckp_data, data);

  gettimeofday (start, NULL);
  stream_factorize (num_blocks, block_size, data, Rstreams, Wstreams, counters);

#pragma omp task input (Wstreams >> final_view[all_blocks][1])
  {
    void * seq_data;
    double stream_time, seq_time;

    gettimeofday (end, NULL);
    stream_time = tdiff (end, start);

    if (! _WITH_SPEEDUPS)
      printf ("%.5f\n", stream_time);
    else
      {
	seq_data = (void * (*)[num_blocks][num_blocks]) calloc (all_blocks, sizeof (void *));
	duplicate (num_blocks, block_size, seq_data, bckp_data);

	gettimeofday (start, NULL);
	sequential_factorize (num_blocks, block_size, seq_data);
	gettimeofday (end, NULL);
        seq_time = tdiff (end, start);

	printf ("Speedup: %.5f \t [seq: %.5f, str: %.5f] -- %d full blocks (%.5f%) \n",
		seq_time / stream_time, seq_time, stream_time, full_blocks, (((float)full_blocks)/((float)all_blocks))*100.0);
      }

    if (_CHECK_STREAM)
      {
	void * l_mat;
	void * u_mat;

	l_mat = (void * (*)[num_blocks][num_blocks]) calloc (all_blocks, sizeof (void *));
	u_mat = (void * (*)[num_blocks][num_blocks]) calloc (all_blocks, sizeof (void *));

	split_matrix (num_blocks, block_size, data, l_mat, u_mat);
	clear_matrix (num_blocks, block_size, data);
	sparse_matmult (num_blocks, block_size, l_mat, u_mat, data);
	matrix_diff (num_blocks, block_size, bckp_data, data);

	// Possibly check that the sequential version was correct
	if (_CHECK_SEQUENTIAL)
	  {
	    clear_matrix (num_blocks, block_size, l_mat);
	    clear_matrix (num_blocks, block_size, u_mat);
	    split_matrix (num_blocks, block_size, seq_data, l_mat, u_mat);
	    clear_matrix (num_blocks, block_size, seq_data);
	    sparse_matmult (num_blocks, block_size, l_mat, u_mat, seq_data);
	    matrix_diff (num_blocks, block_size, bckp_data, seq_data);
	  }
      }
  }

  return 0;
}
