/**
 * Dataflow implementation of seidel.c
 *
 * Copyright (C) 2013 Andi Drebes <andi.drebes@lip6.fr>
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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include <complex.h>
#include "../common/common.h"
#include "../common/sync.h"

#include <unistd.h>

#define _WITH_OUTPUT 1
#define _DUMP_MATRIX 1

#define ___recursive 0
#define ___merge 1

#ifdef _WITH_OUTPUT
  #define verbose_printf printf
#else
  #define verbose_printf(...) do { } while(0)
#endif


/* Init matrix: M[24,24] = M[N-24,N-24] = 500.0
 * (same data as in the sequential version */
static inline double
_matrix (int x, int y, int N)
{
  return ((x == 24 && y == 24) || (x == N-24 && y == N-24))
    ? 500.0
    : 0.0;
}


/* Global stream references (see main for details) */
void* stop_ref;
void* sleft_ref;
void* sright_ref;
void* sbottom_ref;
void* scenter_ref;

void gauss_seidel_df(int blocks_x, int blocks_y, int block_size, int numiters, int it, int id_x, int id_y,
		     double* left_in, double* top_in,
		     double* bottom_in, double* right_in, double* center_in,
		     double* left_out, double* top_out, double* bottom_out,
		     double* right_out, double* center_out);

/* Creates a new task for the next iteration.
 * Input and output dependencies are automatically determined from the
 * block's position.
 *
 * blocks_x, blocks_y: number of blocks in each direction of the matrix
 * block_size: block width / height
 * numiters: total number of iterations
 * it: iteration of the task to be created
 * id_x, id_y: horizontal / vertical position of the block within the matrix
 */
void create_next_iteration_task(int blocks_x, int blocks_y, int block_size, int numiters, int it, int id_x, int id_y)
{
	/* Arrays used to access the data from the streams */
	double top_in[block_size];
	double top_out[block_size];
	double left_in[block_size];
	double left_out[block_size];
	double right_in[block_size];
	double right_out[block_size];
	double bottom_in[block_size];
	double bottom_out[block_size];
	double center_in[block_size*block_size];
	double center_out[block_size*block_size];

	/* block id */
	int id = id_y*blocks_x + id_x;

	/* number of blocks in the matrix */
	int blocks = blocks_x * blocks_y;

	if(id_y == 0 && id_x == 0) {
		/* Upper left corner */
		#pragma omp task input(stop_ref   [it*blocks + (id+blocks_x)] >> bottom_in[block_size],	\
				       sleft_ref  [it*blocks + (id+1)       ] >> right_in[block_size], \
				       scenter_ref[it*blocks + id           ] >> center_in[block_size*block_size]) \
			output(sbottom_ref[(it+1)*blocks+id] << bottom_out[block_size],	\
			       sright_ref [(it+1)*blocks+id] << right_out[block_size], \
			       scenter_ref[(it+1)*blocks+id] << center_out[block_size*block_size]) proc_bind(spread)
		{
			verbose_printf("Task for block (%d, %d) executing.\n", id_x, id_y);
			gauss_seidel_df(blocks_x, blocks_y, block_size, numiters, it, id_x, id_y,
					NULL, NULL, bottom_in, right_in, center_in,
					NULL, NULL, bottom_out, right_out, center_out);
		}
	} else if(id_y == 0 && id_x < blocks_x-1) {
		/* Block in the top row, but not in a corner */
		#pragma omp task input(stop_ref   [it*blocks + (id+blocks_x)] >> bottom_in[block_size],	\
				       sright_ref [(it+1)*blocks + (id-1)   ] >> left_in[block_size], \
				       sleft_ref  [it*blocks + (id+1)       ] >> right_in[block_size], \
				       scenter_ref[it*blocks + id           ] >> center_in[block_size*block_size]) \
			output(sbottom_ref[(it+1)*blocks+id] << bottom_out[block_size],	\
			       sleft_ref  [(it+1)*blocks+id] << left_out[block_size], \
			       sright_ref [(it+1)*blocks+id] << right_out[block_size], \
			       scenter_ref[(it+1)*blocks+id] << center_out[block_size*block_size]) proc_bind(spread)
		{
			verbose_printf("Task for block (%d, %d) executing.\n", id_x, id_y);
			gauss_seidel_df(blocks_x, blocks_y, block_size, numiters, it, id_x, id_y,
					left_in, NULL, bottom_in, right_in, center_in,
					left_out, NULL, bottom_out, right_out, center_out);
		}
	} else if(id_y == 0 && id_x == blocks_x-1) {
		/* Upper right corner */
		#pragma omp task input(stop_ref   [it*blocks + (id+blocks_x)] >> bottom_in[block_size],	\
				       sright_ref [(it+1)*blocks + (id-1)   ] >> left_in[block_size], \
				       scenter_ref[it*blocks + id           ] >> center_in[block_size*block_size]) \
			output(sbottom_ref[(it+1)*blocks + id] << bottom_out[block_size], \
			       sleft_ref  [(it+1)*blocks + id] << left_out[block_size],	\
			       scenter_ref[(it+1)*blocks + id] << center_out[block_size*block_size]) proc_bind(spread)
		{
			verbose_printf("Task for block (%d, %d) executing.\n", id_x, id_y);
			gauss_seidel_df(blocks_x, blocks_y, block_size, numiters, it, id_x, id_y,
					left_in, NULL, bottom_in, NULL, center_in,
					left_out, NULL, bottom_out, NULL, center_out);
		}
	} else if(id_x == 0 && id_y < blocks_y-1) {
		/* Block in the leftmost column, but not in a corner */
		#pragma omp task input(sbottom_ref[(it+1)*blocks + (id-blocks_x)] >> top_in[block_size], \
				       stop_ref   [it*blocks + (id+blocks_x)] >> bottom_in[block_size],	\
				       sleft_ref  [it*blocks + (id+1)       ] >> right_in[block_size], \
				       scenter_ref[it*blocks + id           ] >> center_in[block_size*block_size]) \
			output(stop_ref   [(it+1)*blocks + id] << top_out[block_size], \
			       sbottom_ref[(it+1)*blocks + id] << bottom_out[block_size], \
			       sright_ref [(it+1)*blocks + id] << right_out[block_size], \
			       scenter_ref[(it+1)*blocks + id] << center_out[block_size*block_size]) proc_bind(spread)
		{
			verbose_printf("Task for block (%d, %d) executing.\n", id_x, id_y);
			gauss_seidel_df(blocks_x, blocks_y, block_size, numiters, it, id_x, id_y,
					NULL, top_in, bottom_in, right_in, center_in,
					NULL, top_out, bottom_out, right_out, center_out);
		}
	} else if(id_x == 0 && id_y == blocks_y-1) {
		/* Lower left corner */
		#pragma omp task input(sbottom_ref[(it+1)*blocks + (id-blocks_x)] >> top_in[block_size], \
				       sleft_ref  [it*blocks + (id+1)       ] >> right_in[block_size], \
				       scenter_ref[it*blocks + id           ] >> center_in[block_size*block_size]) \
			output(stop_ref   [(it+1)*blocks + id] << top_out[block_size], \
			       sright_ref [(it+1)*blocks + id] << right_out[block_size], \
			       scenter_ref[(it+1)*blocks + id] << center_out[block_size*block_size]) proc_bind(spread)
		{
			verbose_printf("Task for block (%d, %d) executing.\n", id_x, id_y);
			gauss_seidel_df(blocks_x, blocks_y, block_size, numiters, it, id_x, id_y,
					NULL, top_in, NULL, right_in,
					center_in, NULL, top_out, NULL, right_out, center_out);
		}
	} else if(id_y == blocks_y-1 && id_x < blocks_x-1) {
		/* Block in the lowest row, but not in a corner */
		#pragma omp task input(sbottom_ref[(it+1)*blocks + (id-blocks_x)] >> top_in[block_size], \
				       sright_ref [(it+1)*blocks + (id-1)   ] >> left_in[block_size], \
				       sleft_ref  [it*blocks + (id+1)       ] >> right_in[block_size], \
				       scenter_ref[it*blocks + id           ] >> center_in[block_size*block_size]) \
			output(stop_ref   [(it+1)*blocks + id] << top_out[block_size], \
			       sleft_ref  [(it+1)*blocks + id] << left_out[block_size],	\
			       sright_ref [(it+1)*blocks + id] << right_out[block_size], \
			       scenter_ref[(it+1)*blocks + id] << center_out[block_size*block_size]) proc_bind(spread)
		{
			verbose_printf("Task for block (%d, %d) executing.\n", id_x, id_y);
			gauss_seidel_df(blocks_x, blocks_y, block_size, numiters, it, id_x, id_y,
					left_in, top_in, NULL, right_in, center_in,
					left_out, top_out, NULL, right_out, center_out);
		}
	} else if(id_y == blocks_y-1 && id_x == blocks_x-1) {
		/* Lower right corner */
		#pragma omp task input(sbottom_ref[(it+1)*blocks + (id-blocks_x)] >> top_in[block_size], \
				       sright_ref [(it+1)*blocks + (id-1)       ] >> left_in[block_size], \
				       scenter_ref[it*blocks + id               ] >> center_in[block_size*block_size]) \
			output(stop_ref[(it+1)*blocks+id] << top_out[block_size], \
			       sleft_ref[(it+1)*blocks+id] << left_out[block_size], \
			       scenter_ref[(it+1)*blocks+id] << center_out[block_size*block_size]) proc_bind(spread)
		{
			verbose_printf("Task for block (%d, %d) executing.\n", id_x, id_y);
			gauss_seidel_df(blocks_x, blocks_y, block_size, numiters, it, id_x, id_y,
					left_in, top_in, NULL, NULL, center_in,
					left_out, top_out, NULL, NULL, center_out);
		}
	} else if(id_x == blocks_x-1 && id_y < blocks_y-1) {
		/* Block in the rightmost column, but not in a corner */
		#pragma omp task input(sbottom_ref[(it+1)*blocks + (id-blocks_x)] >> top_in[block_size], \
				       stop_ref   [it*blocks + (id+blocks_x)] >> bottom_in[block_size],	\
				       sright_ref [(it+1)*blocks + (id-1)   ] >> left_in[block_size], \
				       scenter_ref[it*blocks + id           ] >> center_in[block_size*block_size]) \
			output(stop_ref   [(it+1)*blocks + id] << top_out[block_size], \
			       sbottom_ref[(it+1)*blocks + id] << bottom_out[block_size], \
			       sleft_ref  [(it+1)*blocks + id] << left_out[block_size],	\
			       scenter_ref[(it+1)*blocks + id] << center_out[block_size*block_size]) proc_bind(spread)
		{
			verbose_printf("Task for block (%d, %d) executing.\n", id_x, id_y);
			gauss_seidel_df(blocks_x, blocks_y, block_size, numiters, it, id_x, id_y,
					left_in, top_in, bottom_in, NULL, center_in,
					left_out, top_out, bottom_out, NULL, center_out);
		}
	} else {
		/* Block somewhere in the center, not touching any border*/
		#pragma omp task input(sbottom_ref[(it+1)*blocks + (id-blocks_x)] >> top_in[block_size], \
				       stop_ref   [it*blocks + (id+blocks_x)] >> bottom_in[block_size], \
				       sright_ref [(it+1)*blocks + (id-1)       ] >> left_in[block_size], \
				       sleft_ref  [it*blocks + (id+1)       ] >> right_in[block_size], \
				       scenter_ref[it*blocks + id           ] >> center_in[block_size*block_size]) \
			output(stop_ref   [(it+1)*blocks + id] << top_out[block_size], \
			       sbottom_ref[(it+1)*blocks + id] << bottom_out[block_size], \
			       sleft_ref  [(it+1)*blocks + id] << left_out[block_size], \
			       sright_ref [(it+1)*blocks + id] << right_out[block_size], \
			       scenter_ref[(it+1)*blocks + id] << center_out[block_size*block_size]) proc_bind(spread)
		{
			verbose_printf("Task for block (%d, %d) executing.\n", id_x, id_y);
			gauss_seidel_df(blocks_x, blocks_y, block_size, numiters, it, id_x, id_y,
					left_in, top_in, bottom_in, right_in, center_in,
					left_out, top_out, bottom_out, right_out, center_out);
		}
	}
}

/* Performs an iteration on a single block.
 *
 * blocks_x, blocks_y:
 *   number of blocks in each direction of the matrix
 *
 * block_size:
 *   block width / height
 *
 * block_size:
 *   block width / height
 *
 * numiters:
 *   total number of iterations
 *
 * it:
 *   iteration of the task to be created
 * id_x, id_y: horizontal / vertical position of the block within the matrix
 *
 * left_in, top_in:
 *   data produced during the same iteration by the left / upper neighbor
 *
 * right_in, bottom_in, center_in:
 *   data produced during the previous iteration by the right, lower and
 *   upper neighbor and the block itself
 *
 * right_out, bottom_out:
 *   output data of this block for the right / lower neighbor of the same
 *   iteration.
 *
 * left_out, top_out, center_out:
 *   output data of this block for the next iteration
 *
 * NULL values are passed for pointers that are not required
 * because of the block's location (e.g. left_in is NULL for
 * blocks on the left of the matrix; top_out is NULL for blocks
 * at the top of the matrix).
 */
void gauss_seidel_df(int blocks_x, int blocks_y, int block_size, int numiters, int it, int id_x, int id_y,
		     double* left_in, double* top_in,
		     double* bottom_in, double* right_in, double* center_in,
		     double* left_out, double* top_out, double* bottom_out,
		     double* right_out, double* center_out)
{
	/* Depending on the position of an element within the block,
	 * different data sources must be used. For example, elements
	 * on the left side depend on left_in, those on the right side
	 * depend on right_in and so on. The different cases are treated
	 * outside of the main loop in order to avoid branching within
	 * the loop body.
	 *
	 * The loop bodies are very similar for all cases:
	 * First, the input elements top_val, left_val, right_val,
	 * bottom_val and center_val are determined in order to
	 * calculate the new value of the element. Afterwards,
	 * the output matrix is updated.
	 *
	 * If the element is a part of the another block's input at
	 * the next iteration, the appropriate output vector is updated
	 * as well.
	 */

	for(int y = 0; y < block_size; y++) {
		for(int x = 0; x < block_size; x++) {
			double top_val = (y == 0)
				? (top_in ? top_in[x] : 0.0)
				: center_out[(y-1)*block_size + x];
			double left_val = (x == 0)
				? (left_in ? left_in[y] : 0.0)
				: center_out[y*block_size + (x-1)];
			double right_val = (x == block_size-1)
				? (right_in ? right_in[y] : 0.0)
				: center_in[y*block_size + (x + 1)];
			double bottom_val = (y == block_size-1)
				? (bottom_in ? bottom_in[x] : 0.0)
				: center_in[(y+1)*block_size + x];
			double center_val = center_in[y*block_size+x];
			double new_val = (top_val + left_val + right_val +
					  bottom_val + center_val) * 0.2;

			if(x == block_size-1 && right_out)
				right_out[y] = new_val;
			if(x == 0 && left_out)
				left_out[y] = new_val;
			if(y == block_size-1 && bottom_out)
				bottom_out[x] = new_val;
			if(y == 0 && top_out)
				top_out[x] = new_val;

			center_out[y*block_size+x] = new_val;
		}
	}

	/* Recursively create a new task for this block.
	 * Note that the task for the next iteration directly depends of
	 * center_out. Therefore, the task to be created cannot be the one
	 * for the next iteration. The minimal distance is two.
	 */
#if ___recursive
	if(it+2 < numiters)
	  create_next_iteration_task(blocks_x, blocks_y, block_size, numiters, it+2, id_x, id_y);
#endif
}


/* Copies data of a block of the initial matrix into
 * the vectors and the matrix of the output stream.
 *
 * matrix: Initial matrix in row-major order
 * id_x, id_y: Block coordinates
 * N: width / height of the matrix
 * block_size: width / height of a block
 * left_out, top_out, bottom_out, right_out, center_out:
 *   output data of this block for the next iteration
 */
void gauss_seidel_df_init(int id_x, int id_y, int N,
			  int block_size, double* left_out, double* top_out,
			  double* bottom_out, double* right_out,
			  double* center_out)
{
	for(int x = 0; x < block_size; x++) {
		for(int y = 0; y < block_size; y++) {
			int global_x = id_x*block_size+x;
			int global_y = id_y*block_size+y;

			double curr = _matrix (global_y, global_x, N);

			center_out[y*block_size+x] = curr;

			if(x == 0 && left_out)
				left_out[y] = curr;
			else if(x == block_size-1 && right_out)
				right_out[y] = curr;

			if(y == 0 && top_out)
				top_out[x] = curr;
			else if(y == block_size-1 && bottom_out)
				bottom_out[x] = curr;
		}
	}
}

/* Inverse operation of gauss_seidel_df_init:
 * Copies the final data from the stream buffers into a matrix.
 *
 * matrix: Final matrix in row-major order
 * id_x, id_y: Block coordinates
 * N: width / height of the matrix
 * block_size: width / height of a block
 * left_in, top_in, bottom_in, right_in, center_in:
 *   output data of the blocks at the last iteration
 */
 extern long long int node_id;

void dump_matrix(FILE* fp, int N, double* matrix);

#if ___merge
void gauss_seidel_df_finish(double *out_block, int id_x, int id_y, int N,
			    int block_size, double* left_in, double* top_in,
			    double* bottom_in, double* right_in,
			    double* center_in)
#else
void gauss_seidel_df_finish(int id_x, int id_y, int N,
			    int block_size, double* left_in, double* top_in,
			    double* bottom_in, double* right_in,
			    double* center_in)
#endif
{
#if !___merge
  char name[256];
  sprintf(name, "%s/outputs/DF_seidel.out.%03d.%03d", getenv("PWD"), id_x, id_y);
  //fprintf (stderr, "%s\n", name);
  FILE *out = fopen (name, "w");

  double matrix[block_size * block_size];
#endif

  for(int x = 0; x < block_size; x++) {
    for(int y = 0; y < block_size; y++) {
      double curr = center_in[y*block_size+x];

      int global_x = id_x*block_size+x;
      int global_y = id_y*block_size+y;

      if(x == 0 && left_in)
	curr = left_in[y];
      else if(x == block_size-1 && right_in)
	curr = right_in[y];

      if(y == 0 && top_in)
	curr = top_in[x];
      else if(y == block_size-1 && bottom_in)
	curr = bottom_in[x];

#if ___merge
      out_block[y*block_size + x] = curr;
#else
      matrix[y*block_size + x] = curr;
#endif
    }
  }

}

/* Writes the contents of a matrix in
 * row-major order to a file.
 */
void dump_matrix(FILE* fp, int N, double* matrix)
{
	for(int x = 0; x < N+2; x++)
		fprintf(fp, "%f \t", 0.0);
	fprintf(fp, "\n");

	for(int y = 0; y < N; y++) {
		fprintf(fp, "%f \t", 0.0);
		for(int x = 0; x < N; x++)
			fprintf(fp, "%f \t", matrix[y*N+x]);
		fprintf(fp, "%f \t", 0.0);

		fprintf(fp, "\n");
	}

	for(int x = 0; x < N+2; x++)
		fprintf(fp, "%f \t", 0.0);
	fprintf(fp, "\n");
}

//struct profiler_sync sync;

int main(int argc, char** argv)
{
	int N = (1 << 13);
	int block_size = (1 << 8);
	int numiters = 60;

	struct timeval start;
	struct timeval end;

	int option;

	/* Parse options */
	while ((option = getopt(argc, argv, "n:s:b:r:h")) != -1)
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
			case 'h':
				printf("Usage: %s [option]...\n\n"
				       "Options:\n"
				       "  -n <size>                    Number of colums of the square matrix, default is %d\n"
				       "  -s <power>                   Set the number of colums of the square matrix to 1 << <power>\n"
				       "  -b <block size power>        Set the block size 1 << <block size power>, default is %d\n"
				       "  -r <iterations>              Number of iterations\n"
				       "  -o <output file>             Write data to output file, default is stream_seidel_df.out\n",
				       argv[0], N, block_size);
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

	//PROFILER_NOTIFY_PREPARE(&sync);

	///double* matrix = malloc(N*N*sizeof(double));

	int blocks_x = N / block_size;
	int blocks_y = N / block_size;
	int blocks = blocks_x * blocks_y;

	/* A quintuple of streams is associated to each block for
	 * each iteration. The naming scheme for the streams is
	 * producer-centric (e.g. a task writes its right-side values
	 * into sright at iteration n; its right neighbor reads these
	 * values at iteration n+1.
	 */
	double stop[(numiters+1)*blocks] __attribute__((stream));
	double sleft[(numiters+1)*blocks] __attribute__((stream));
	double sright[(numiters+1)*blocks] __attribute__((stream));
	double sbottom[(numiters+1)*blocks] __attribute__((stream));
	double scenter[(numiters+1)*blocks] __attribute__((stream));

#if ___merge
	double sfinal __attribute__((stream));
#endif

	/* As tasks are created recursively, a stream reference has to be
	 * passed to the tasks. This could be done using a stream in
	 * which case the references would be copied for each task. As
	 * the number of streams cqn be quite high, this would cause
	 * high overhead.
	 * Instead, the local table is copied into a global table which is
	 * directly accessed by the tasks.
	 */
	stop_ref = malloc((numiters+1)*blocks * sizeof (void *));
	sleft_ref = malloc((numiters+1)*blocks * sizeof (void *));
	sright_ref = malloc((numiters+1)*blocks * sizeof (void *));
	sbottom_ref = malloc((numiters+1)*blocks * sizeof (void *));
	scenter_ref = malloc((numiters+1)*blocks * sizeof (void *));

	/* Copy the local table into the global one */
	memcpy (stop_ref, stop, (numiters+1)*blocks * sizeof (void *));
	memcpy (sleft_ref, sleft, (numiters+1)*blocks * sizeof (void *));
	memcpy (sright_ref, sright, (numiters+1)*blocks * sizeof (void *));
	memcpy (sbottom_ref, sbottom, (numiters+1)*blocks * sizeof (void *));
	memcpy (scenter_ref, scenter, (numiters+1)*blocks * sizeof (void *));

	/* Arrays used to access the data from the streams at
	 * initialization / termination */
	double top_in[block_size];
	double top_out[block_size];
	double left_in[block_size];
	double left_out[block_size];
	double center_in[block_size*block_size];
	double center_out[block_size*block_size];
#if ___merge
	double block_out[block_size*block_size];
	double matrix_in[N*N];
#endif

	/* Init matrix: M[24,24] = M[N-24,N-24] = 500.0
	 * (same data as in the sequential version */
	///matrix[24*N+24] = 500.0;
	///matrix[(N-24)*N+(N-24)] = 500.0;

	gettimeofday(&start, NULL);
	//PROFILER_NOTIFY_RECORD(&sync);

	/* Create tasks that initialize the streams and those that
	 * read the final value from the streams.
	 * As the dependencies rely on the position of the block,
	 * all cases are treated explicitly.
	 */

	for(int id_x = 0; id_x < blocks_x; id_x++) {
		for(int id_y = 0; id_y < blocks_y; id_y++) {
			int id = id_y*blocks_x + id_x;

			/* Upper left corner */
			if(id_y == 0 && id_x == 0) {
				#pragma omp task output(scenter[id] << center_out[block_size*block_size])
				{
					gauss_seidel_df_init(id_x, id_y, N, block_size,
							     NULL, NULL, NULL, NULL, center_out);
				}
#if ___merge
				#pragma omp task input(scenter[numiters*blocks+id] >> center_in[block_size*block_size]) \
				  output (sfinal << block_out[block_size * block_size])
				{
				  gauss_seidel_df_finish(block_out, id_x, id_y, N, block_size,
							 NULL, NULL, NULL, NULL, center_in);
				}
#else
				#pragma omp task input(scenter[numiters*blocks+id] >> center_in[block_size*block_size])
				{
					gauss_seidel_df_finish(id_x, id_y, N, block_size,
							       NULL, NULL, NULL, NULL, center_in);
				}
#endif
			} else if(id_y == 0 && id_x < blocks_x-1) {
				/* Upper row, but not in a corner  */
				#pragma omp task output(sleft[id] << left_out[block_size],     \
							scenter[id] << center_out[block_size*block_size])
				{
					gauss_seidel_df_init(id_x, id_y, N, block_size,
							     left_out, NULL, NULL, NULL, center_out);
				}

#if ___merge
				#pragma omp task input(sleft  [numiters*blocks+id] >> left_in[block_size],   \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size]) \
				  output (sfinal << block_out[block_size * block_size])
				{
					gauss_seidel_df_finish(block_out, id_x, id_y, N, block_size,
							       left_in, NULL, NULL, NULL, center_in);
				}
#else
				#pragma omp task input(sleft  [numiters*blocks+id] >> left_in[block_size],   \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size])
				{
					gauss_seidel_df_finish(id_x, id_y, N, block_size,
							       left_in, NULL, NULL, NULL, center_in);
				}
#endif
			} else if(id_y == 0 && id_x == blocks_x-1) {
				/* Upper right corner */
				#pragma omp task output(sleft[id] << left_out[block_size],     \
							scenter[id] << center_out[block_size*block_size])
				{
					gauss_seidel_df_init(id_x, id_y, N, block_size,
							     left_out, NULL, NULL, NULL, center_out);
				}

#if ___merge
				#pragma omp task input(sleft  [numiters*blocks+id] >> left_in[block_size],   \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size]) \
				  output (sfinal << block_out[block_size * block_size])
				{
					gauss_seidel_df_finish(block_out, id_x, id_y, N, block_size,
							       left_in, NULL, NULL, NULL, center_in);
				}
#else
				#pragma omp task input(sleft  [numiters*blocks+id] >> left_in[block_size],   \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size])
				{
					gauss_seidel_df_finish(id_x, id_y, N, block_size,
							       left_in, NULL, NULL, NULL, center_in);
				}
#endif
			} else if(id_x == 0 && id_y < blocks_y-1) {
				/* Left row, but not in a corner*/
				#pragma omp task output(stop[id] << top_out[block_size],       \
							scenter[id] << center_out[block_size*block_size])
				{
					gauss_seidel_df_init(id_x, id_y, N, block_size,
							     NULL, top_out, NULL, NULL, center_out);
				}

#if ___merge
				#pragma omp task input(stop   [numiters*blocks+id] >> top_in[block_size],    \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size]) \
				  output (sfinal << block_out[block_size * block_size])
				{
					gauss_seidel_df_finish(block_out, id_x, id_y, N, block_size,
							       NULL, top_in, NULL, NULL, center_in);
				}
#else
				#pragma omp task input(stop   [numiters*blocks+id] >> top_in[block_size],    \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size])
				{
					gauss_seidel_df_finish(id_x, id_y, N, block_size,
							       NULL, top_in, NULL, NULL, center_in);
				}
#endif
			} else if(id_x == 0 && id_y == blocks_y-1) {
				/* Lower left corner */
				#pragma omp task output(stop[id] << top_out[block_size],     \
							scenter[id] << center_out[block_size*block_size])
				{
					gauss_seidel_df_init(id_x, id_y, N, block_size,
							     NULL, top_out, NULL, NULL, center_out);
				}

#if ___merge
				#pragma omp task input(stop   [numiters*blocks+id] >> top_in[block_size],   \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size]) \
				  output (sfinal << block_out[block_size * block_size])
				{
					gauss_seidel_df_finish(block_out, id_x, id_y, N, block_size,
							       NULL, top_in, NULL, NULL, center_in);
				}
#else
				#pragma omp task input(stop   [numiters*blocks+id] >> top_in[block_size],   \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size])
				{
					gauss_seidel_df_finish(id_x, id_y, N, block_size,
							       NULL, top_in, NULL, NULL, center_in);
				}
#endif
			} else if(id_y == blocks_y-1 && id_x < blocks_x-1) {
				/* Lower row, but not in a corner */
				#pragma omp task output(sleft[id] << left_out[block_size],   \
							stop[id] << top_out[block_size],     \
							scenter[id] << center_out[block_size*block_size])
				{
					gauss_seidel_df_init(id_x, id_y, N, block_size,
							     left_out, top_out, NULL, NULL, center_out);
				}

#if ___merge
				#pragma omp task input(sleft  [numiters*blocks+id] >> left_in[block_size],  \
						       stop   [numiters*blocks+id] >> top_in[block_size],   \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size]) \
				  output (sfinal << block_out[block_size * block_size])
				{
					gauss_seidel_df_finish(block_out, id_x, id_y, N, block_size,
							       left_in, top_in, NULL, NULL, center_in);
				}
#else
				#pragma omp task input(sleft  [numiters*blocks+id] >> left_in[block_size],  \
						       stop   [numiters*blocks+id] >> top_in[block_size],   \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size])
				{
					gauss_seidel_df_finish(id_x, id_y, N, block_size,
							       left_in, top_in, NULL, NULL, center_in);
				}
#endif
			} else if(id_y == blocks_y-1 && id_x == blocks_x-1) {
				/* Lower right corner */
				#pragma omp task output(sleft[id] << left_out[block_size], \
							stop[id] << top_out[block_size],   \
							scenter[id] << center_out[block_size*block_size])
				{
					gauss_seidel_df_init(id_x, id_y, N, block_size,
							     left_out, top_out, NULL, NULL, center_out);
				}

#if ___merge
				#pragma omp task input(sleft[numiters*blocks+id] >> left_in[block_size], \
						       stop [numiters*blocks+id] >> top_in[block_size],  \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size]) \
				  output (sfinal << block_out[block_size * block_size])
				{
					gauss_seidel_df_finish(block_out, id_x, id_y, N, block_size,
							       left_in, top_in, NULL, NULL, center_in);
				}
#else
				#pragma omp task input(sleft[numiters*blocks+id] >> left_in[block_size], \
						       stop [numiters*blocks+id] >> top_in[block_size],  \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size])
				{
					gauss_seidel_df_finish(id_x, id_y, N, block_size,
							       left_in, top_in, NULL, NULL, center_in);
				}
#endif
			} else if(id_x == blocks_x-1 && id_y < blocks_y-1) {
				/* Right row, but not in a corner */
				#pragma omp task output(sleft  [id] << left_out[block_size],   \
							stop   [id] << top_out[block_size],    \
							scenter[id] << center_out[block_size*block_size])
				{
					gauss_seidel_df_init(id_x, id_y, N, block_size,
							     left_out, top_out, NULL, NULL, center_out);
				}

#if ___merge
				#pragma omp task input(sleft  [numiters*blocks+id] >> left_in[block_size],   \
						       stop   [numiters*blocks+id] >> top_in[block_size],    \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size]) \
				  output (sfinal << block_out[block_size * block_size])
				{
					gauss_seidel_df_finish(block_out, id_x, id_y, N, block_size,
							       left_in, top_in, NULL, NULL, center_in);
				}
#else
				#pragma omp task input(sleft  [numiters*blocks+id] >> left_in[block_size],   \
						       stop   [numiters*blocks+id] >> top_in[block_size],    \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size])
				{
					gauss_seidel_df_finish(id_x, id_y, N, block_size,
							       left_in, top_in, NULL, NULL, center_in);
				}
#endif
			} else {
				/* Block in the center */
				#pragma omp task output(sleft[id] << left_out[block_size],     \
							stop[id] << top_out[block_size],       \
							scenter[id] << center_out[block_size*block_size])
				{
					gauss_seidel_df_init(id_x, id_y, N, block_size,
							     left_out, top_out, NULL, NULL, center_out);
				}

#if ___merge
				#pragma omp task input(sleft  [numiters*blocks+id] >> left_in[block_size],   \
						       stop   [numiters*blocks+id] >> top_in[block_size],    \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size]) \
				  output (sfinal << block_out[block_size * block_size])
				{
					gauss_seidel_df_finish(block_out, id_x, id_y, N, block_size,
							       left_in, top_in, NULL, NULL, center_in);
				}
#else
				#pragma omp task input(sleft  [numiters*blocks+id] >> left_in[block_size],   \
						       stop   [numiters*blocks+id] >> top_in[block_size],    \
						       scenter[numiters*blocks+id] >> center_in[block_size*block_size])
				{
					gauss_seidel_df_finish(id_x, id_y, N, block_size,
							       left_in, top_in, NULL, NULL, center_in);
				}
#endif
			}
		}
	}


#if ___merge
#pragma omp task input (sfinal >> matrix_in[N*N])
	{
	  #if _DUMP_MATRIX
	  FILE *r_file = fopen("stream_seidel_df.out", "w");
	  #endif

	  double* matrix = malloc(N * N * sizeof(double));

	  assert(matrix);

	  for(int id_x = 0; id_x < blocks_x; id_x++) {
	    for(int id_y = 0; id_y < blocks_y; id_y++)
	      {
		for(int x = 0; x < block_size; x++) {
		  for(int y = 0; y < block_size; y++) {

		    int global_x = id_x*block_size+x;
		    int global_y = id_y*block_size+y;

		    int stream_pos = (id_x * blocks_y + id_y) * block_size * block_size;

		    matrix[global_y*N+global_x] = matrix_in[stream_pos + y*block_size + x];
		  }
		}
	      }
	  }

	  #if _DUMP_MATRIX
	  assert(r_file);
	  dump_matrix (r_file, N, matrix);
	  fclose (r_file);
	  #endif

	  free(matrix);
	}
#endif

	/* As there are dependencies between a block's task and the same task
	 * at the next iteration, recursive task creation as in gauss_seidel_df()
	 * must have an iteration distance of at least two. However, if there
	 * are less than two iterations altogether, then only one initial task
	 * per block has to be created.
	 */
#if ___recursive
	int init_iter = (numiters > 1) ? 2 : numiters;
#else
	int init_iter = numiters;
#endif

	/* Start the per-block-per-iteration tasks */
	for(int it = 0; it <init_iter; it++) {
	  for(int id_y = 0; id_y < blocks_y; id_y++) {
	    for(int id_x = 0; id_x < blocks_x; id_x++) {
	      create_next_iteration_task(blocks_x, blocks_y, block_size, numiters, it, id_x, id_y);
	    }
	  }
	}

	/* Wait for all the tasks to finish */
	#pragma omp taskwait

	//PROFILER_NOTIFY_PAUSE(&sync);

	gettimeofday(&end, NULL);

	//PROFILER_NOTIFY_FINISH(&sync);

	printf("%.5f\n", tdiff(&end, &start));

	#if _WITH_OUTPUT
	printf("[Dataflow] Seidel (size %d, tile %d, iterations %d) executed in %.5f seconds\n",
		N, block_size, numiters, tdiff(&end, &start));
	#endif

	free(stop_ref);
	free(sleft_ref);
	free(sright_ref);
	free(sbottom_ref);
	free(scenter_ref);

	return 0;
}
