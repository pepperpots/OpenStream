/**
 * SHM implementation of seidel.c
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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "../common/common.h"
#include "../common/sync.h"

#define _WITH_OUTPUT 0
#define _WITH_BINARY_OUTPUT 0

/* Global stream references (see main for details) */
void* stop_ref;
void* sleft_ref;
void* sright_ref;
void* sbottom_ref;
void* scenter_ref;
void* sdfbarrier_ref;

void gauss_seidel_df_in_place_unrolled(double* matrix, int blocks_x, int blocks_y, int block_size, int id_x, int id_y);
void create_next_iteration_task(double* matrix, int blocks_x, int blocks_y, int block_size, int numiters, int it, int id_x, int id_y);
void create_terminal_task(double* matrix, int N, int numiters, int block_size, int id_x, int id_y);

void create_iter_followup_task(double* matrix, int blocks_x, int blocks_y, int block_size, int numiters, int it, int id_x, int id_y)
{
	int N = blocks_x * block_size;

	/* Recursively create a new task for this block.
	 * Note that the task for the next iteration directly depends of
	 * center_out. Therefore, the task to be created cannot be the one
	 * for the next iteration. The minimal distance is two.
	 */
	if(it+2 < numiters)
		create_next_iteration_task(matrix, blocks_x, blocks_y, block_size, numiters, it+2, id_x, id_y);

	if(it == numiters-2)
		create_terminal_task(matrix, N, numiters, block_size, id_x, id_y);
}

void create_init_followup_task(double* matrix, int blocks_x, int blocks_y, int block_size, int numiters, int id_x, int id_y)
{
	int N = blocks_x * block_size;

	if(numiters > 2)
		create_next_iteration_task(matrix, blocks_x, blocks_y, block_size, numiters, 1, id_x, id_y);

	if(numiters == 2)
		create_terminal_task(matrix, N, numiters, block_size, id_x, id_y);
}

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
void create_next_iteration_task(double* matrix, int blocks_x, int blocks_y, int block_size, int numiters, int it, int id_x, int id_y)
{
	/* Arrays used to access the data from the streams */
	double top_in[1];
	double top_out[1];
	double left_in[1];
	double left_out[1];
	double right_in[1];
	double right_out[1];
	double bottom_in[1];
	double bottom_out[1];
	double center_in[1];
	double center_out[1];

	/* block id */
	int id = id_y*blocks_x + id_x;

	/* number of blocks in the matrix */
	int blocks = blocks_x * blocks_y;

	if(id_y == 0 && id_x == 0) {
		/* Upper left corner */
		#pragma omp task input(stop_ref   [it*blocks + (id+blocks_x)] >> bottom_in[1],	\
				       sleft_ref  [it*blocks + (id+1)       ] >> right_in[1], \
				       scenter_ref[it*blocks + id           ] >> center_in[1]) \
			output(sbottom_ref[(it+1)*blocks+id] << bottom_out[1],	\
			       sright_ref [(it+1)*blocks+id] << right_out[1], \
			       scenter_ref[(it+1)*blocks+id] << center_out[1])
		{
			gauss_seidel_df_in_place_unrolled(matrix, blocks_x, blocks_y, block_size, id_x, id_y);
			create_iter_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, it, id_x, id_y);
		}
	} else if(id_y == 0 && id_x < blocks_x-1) {
		/* Block in the top row, but not in a corner */
		#pragma omp task input(stop_ref   [it*blocks + (id+blocks_x)] >> bottom_in[1],	\
				       sright_ref [(it+1)*blocks + (id-1)   ] >> left_in[1], \
				       sleft_ref  [it*blocks + (id+1)       ] >> right_in[1], \
				       scenter_ref[it*blocks + id           ] >> center_in[1]) \
			output(sbottom_ref[(it+1)*blocks+id] << bottom_out[1],	\
			       sleft_ref  [(it+1)*blocks+id] << left_out[1], \
			       sright_ref [(it+1)*blocks+id] << right_out[1], \
			       scenter_ref[(it+1)*blocks+id] << center_out[1])
		{
			gauss_seidel_df_in_place_unrolled(matrix, blocks_x, blocks_y, block_size, id_x, id_y);
			create_iter_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, it, id_x, id_y);
		}
	} else if(id_y == 0 && id_x == blocks_x-1) {
		/* Upper right corner */
		#pragma omp task input(stop_ref   [it*blocks + (id+blocks_x)] >> bottom_in[1],	\
				       sright_ref [(it+1)*blocks + (id-1)   ] >> left_in[1], \
				       scenter_ref[it*blocks + id           ] >> center_in[1]) \
			output(sbottom_ref[(it+1)*blocks + id] << bottom_out[1], \
			       sleft_ref  [(it+1)*blocks + id] << left_out[1],	\
			       scenter_ref[(it+1)*blocks + id] << center_out[1])
		{
			gauss_seidel_df_in_place_unrolled(matrix, blocks_x, blocks_y, block_size, id_x, id_y);
			create_iter_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, it, id_x, id_y);
		}
	} else if(id_x == 0 && id_y < blocks_y-1) {
		/* Block in the leftmost column, but not in a corner */
		#pragma omp task input(sbottom_ref[(it+1)*blocks + (id-blocks_x)] >> top_in[1], \
				       stop_ref   [it*blocks + (id+blocks_x)] >> bottom_in[1],	\
				       sleft_ref  [it*blocks + (id+1)       ] >> right_in[1], \
				       scenter_ref[it*blocks + id           ] >> center_in[1]) \
			output(stop_ref   [(it+1)*blocks + id] << top_out[1], \
			       sbottom_ref[(it+1)*blocks + id] << bottom_out[1], \
			       sright_ref [(it+1)*blocks + id] << right_out[1], \
			       scenter_ref[(it+1)*blocks + id] << center_out[1])
		{
			gauss_seidel_df_in_place_unrolled(matrix, blocks_x, blocks_y, block_size, id_x, id_y);
			create_iter_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, it, id_x, id_y);
		}
	} else if(id_x == 0 && id_y == blocks_y-1) {
		/* Lower left corner */
		#pragma omp task input(sbottom_ref[(it+1)*blocks + (id-blocks_x)] >> top_in[1], \
				       sleft_ref  [it*blocks + (id+1)       ] >> right_in[1], \
				       scenter_ref[it*blocks + id           ] >> center_in[1]) \
			output(stop_ref   [(it+1)*blocks + id] << top_out[1], \
			       sright_ref [(it+1)*blocks + id] << right_out[1], \
			       scenter_ref[(it+1)*blocks + id] << center_out[1])
		{
			gauss_seidel_df_in_place_unrolled(matrix, blocks_x, blocks_y, block_size, id_x, id_y);
			create_iter_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, it, id_x, id_y);
		}
	} else if(id_y == blocks_y-1 && id_x < blocks_x-1) {
		/* Block in the lowest row, but not in a corner */
		#pragma omp task input(sbottom_ref[(it+1)*blocks + (id-blocks_x)] >> top_in[1], \
				       sright_ref [(it+1)*blocks + (id-1)   ] >> left_in[1], \
				       sleft_ref  [it*blocks + (id+1)       ] >> right_in[1], \
				       scenter_ref[it*blocks + id           ] >> center_in[1]) \
			output(stop_ref   [(it+1)*blocks + id] << top_out[1], \
			       sleft_ref  [(it+1)*blocks + id] << left_out[1],	\
			       sright_ref [(it+1)*blocks + id] << right_out[1], \
			       scenter_ref[(it+1)*blocks + id] << center_out[1])
		{
			gauss_seidel_df_in_place_unrolled(matrix, blocks_x, blocks_y, block_size, id_x, id_y);
			create_iter_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, it, id_x, id_y);
		}
	} else if(id_y == blocks_y-1 && id_x == blocks_x-1) {
		/* Lower right corner */
		#pragma omp task input(sbottom_ref[(it+1)*blocks + (id-blocks_x)] >> top_in[1], \
				       sright_ref [(it+1)*blocks + (id-1)       ] >> left_in[1], \
				       scenter_ref[it*blocks + id               ] >> center_in[1]) \
			output(stop_ref[(it+1)*blocks+id] << top_out[1], \
			       sleft_ref[(it+1)*blocks+id] << left_out[1], \
			       scenter_ref[(it+1)*blocks+id] << center_out[1])
		{
			gauss_seidel_df_in_place_unrolled(matrix, blocks_x, blocks_y, block_size, id_x, id_y);
			create_iter_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, it, id_x, id_y);
		}
	} else if(id_x == blocks_x-1 && id_y < blocks_y-1) {
		/* Block in the rightmost column, but not in a corner */
		#pragma omp task input(sbottom_ref[(it+1)*blocks + (id-blocks_x)] >> top_in[1], \
				       stop_ref   [it*blocks + (id+blocks_x)] >> bottom_in[1],	\
				       sright_ref [(it+1)*blocks + (id-1)   ] >> left_in[1], \
				       scenter_ref[it*blocks + id           ] >> center_in[1]) \
			output(stop_ref   [(it+1)*blocks + id] << top_out[1], \
			       sbottom_ref[(it+1)*blocks + id] << bottom_out[1], \
			       sleft_ref  [(it+1)*blocks + id] << left_out[1],	\
			       scenter_ref[(it+1)*blocks + id] << center_out[1])
		{
			gauss_seidel_df_in_place_unrolled(matrix, blocks_x, blocks_y, block_size, id_x, id_y);
			create_iter_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, it, id_x, id_y);
		}
	} else {
		/* Block somewhere in the center, not touching any border*/
		#pragma omp task input(sbottom_ref[(it+1)*blocks + (id-blocks_x)] >> top_in[1], \
				       stop_ref   [it*blocks + (id+blocks_x)] >> bottom_in[1], \
				       sright_ref [(it+1)*blocks + (id-1)       ] >> left_in[1], \
				       sleft_ref  [it*blocks + (id+1)       ] >> right_in[1], \
				       scenter_ref[it*blocks + id           ] >> center_in[1]) \
			output(stop_ref   [(it+1)*blocks + id] << top_out[1], \
			       sbottom_ref[(it+1)*blocks + id] << bottom_out[1], \
			       sleft_ref  [(it+1)*blocks + id] << left_out[1], \
			       sright_ref [(it+1)*blocks + id] << right_out[1], \
			       scenter_ref[(it+1)*blocks + id] << center_out[1])
		{
			gauss_seidel_df_in_place_unrolled(matrix, blocks_x, blocks_y, block_size, id_x, id_y);
			create_iter_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, it, id_x, id_y);
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

void dump_matrix_binary(FILE* fp, int N, double* matrix)
{
	fwrite(&N, sizeof(N), 1, fp);
	fwrite(matrix, N*N*sizeof(double), 1, fp);
}

/* Creates an initial task for a given block. The task reads data
 * from the initial matrix and copies it to the streams.
 */
void create_initial_task(double* matrix, int N, int numiters, int block_size, int id_x, int id_y)
{
	int blocks_x = N / block_size;
	int blocks_y = N / block_size;

	/* Arrays used to access the data from the streams at
	 * initialization / termination */
	double top_out[1];
	double left_out[1];
	double center_out[1];

	int id = id_y*blocks_x + id_x;

	/* Upper left corner */
	if(id_y == 0 && id_x == 0) {
		#pragma omp task output(scenter_ref[id] << center_out[1])
		{
			create_init_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, id_x, id_y);
		}
	} else if(id_y == 0 && id_x < blocks_x-1) {
		/* Upper row, but not in a corner  */
		#pragma omp task output(sleft_ref[id] << left_out[1],		\
			scenter_ref[id] << center_out[1])
		{
			create_init_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, id_x, id_y);
		}
	} else if(id_y == 0 && id_x == blocks_x-1) {
		/* Upper right corner */
		#pragma omp task output(sleft_ref[id] << left_out[1],		\
			scenter_ref[id] << center_out[1])
		{
			create_init_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, id_x, id_y);
		}
	} else if(id_x == 0 && id_y < blocks_y-1) {
		/* Left row, but not in a corner*/
		#pragma omp task output(stop_ref[id] << top_out[1],		\
			scenter_ref[id] << center_out[1])
		{
			create_init_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, id_x, id_y);
		}
	} else if(id_x == 0 && id_y == blocks_y-1) {
		/* Lower left corner */
		#pragma omp task output(stop_ref[id] << top_out[1],		\
			scenter_ref[id] << center_out[1])
		{
			create_init_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, id_x, id_y);
		}
	} else if(id_y == blocks_y-1 && id_x < blocks_x-1) {
		/* Lower row, but not in a corner */
		#pragma omp task output(sleft_ref[id] << left_out[1],		\
			stop_ref[id] << top_out[1],		\
			scenter_ref[id] << center_out[1])
		{
			create_init_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, id_x, id_y);
		}
	} else if(id_y == blocks_y-1 && id_x == blocks_x-1) {
		/* Lower right corner */
		#pragma omp task output(sleft_ref[id] << left_out[1],		\
			stop_ref[id] << top_out[1],		\
			scenter_ref[id] << center_out[1])
		{
			create_init_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, id_x, id_y);
		}
	} else if(id_x == blocks_x-1 && id_y < blocks_y-1) {
		/* Right row, but not in a corner */
		#pragma omp task output(sleft_ref[id] << left_out[1],		\
			stop_ref[id] << top_out[1],		\
			scenter_ref[id] << center_out[1])
		{
			create_init_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, id_x, id_y);
		}
	} else {
		/* Block in the center */
		#pragma omp task output(sleft_ref[id] << left_out[1],		\
			stop_ref[id] << top_out[1],		\
			scenter_ref[id] << center_out[1])
		{
			create_init_followup_task(matrix, blocks_x, blocks_y, block_size, numiters, id_x, id_y);
		}
	}
}

/* Creates a terminal task for a given block. Data is read from the
 * streams and written to the final matrix. */
void create_terminal_task(double* matrix, int N, int numiters, int block_size, int id_x, int id_y)
{
	int blocks_x = N / block_size;
	int blocks_y = N / block_size;
	int blocks = blocks_x * blocks_y;

	/* Arrays used to access the data from the streams at
	 * initialization / termination */
	double top_in[1];
	double left_in[1];
	double center_in[1];
	int token[1];

	int id = id_y*blocks_x + id_x;

	/* Upper left corner */
	if(id_y == 0 && id_x == 0) {
		#pragma omp task input(scenter_ref[numiters*blocks+id] >> center_in[1]) \
			output(sdfbarrier_ref[0] << token[1])
		{
		}
	} else if(id_y == 0 && id_x < blocks_x-1) {
		/* Upper row, but not in a corner  */
		#pragma omp task input(sleft_ref[numiters*blocks+id] >> left_in[1], \
		       scenter_ref[numiters*blocks+id] >> center_in[1]) \
			output(sdfbarrier_ref[0] << token[1])
		{
		}
	} else if(id_y == 0 && id_x == blocks_x-1) {
		/* Upper right corner */
		#pragma omp task input(sleft_ref[numiters*blocks+id] >> left_in[1], \
		       scenter_ref[numiters*blocks+id] >> center_in[1]) \
			output(sdfbarrier_ref[0] << token[1])
		{
		}
	} else if(id_x == 0 && id_y < blocks_y-1) {
		/* Left row, but not in a corner*/
		#pragma omp task input(stop_ref[numiters*blocks+id] >> top_in[1], \
		       scenter_ref[numiters*blocks+id] >> center_in[1]) \
			output(sdfbarrier_ref[0] << token[1])
		{
		}
	} else if(id_x == 0 && id_y == blocks_y-1) {
		/* Lower left corner */
		#pragma omp task input(stop_ref[numiters*blocks+id] >> top_in[1], \
		       scenter_ref[numiters*blocks+id] >> center_in[1]) \
			output(sdfbarrier_ref[0] << token[1])
		{
		}
	} else if(id_y == blocks_y-1 && id_x < blocks_x-1) {
		/* Lower row, but not in a corner */
		#pragma omp task input(sleft_ref[numiters*blocks+id] >> left_in[1], \
		       stop_ref[numiters*blocks+id] >> top_in[1], \
		       scenter_ref[numiters*blocks+id] >> center_in[1]) \
			output(sdfbarrier_ref[0] << token[1])
		{
		}
	} else if(id_y == blocks_y-1 && id_x == blocks_x-1) {
		/* Lower right corner */
		#pragma omp task input(sleft_ref[numiters*blocks+id] >> left_in[1], \
		       stop_ref[numiters*blocks+id] >> top_in[1],	\
		       scenter_ref[numiters*blocks+id] >> center_in[1]) \
			output(sdfbarrier_ref[0] << token[1])
		{
		}
	} else if(id_x == blocks_x-1 && id_y < blocks_y-1) {
		/* Right row, but not in a corner */
		#pragma omp task input(sleft_ref[numiters*blocks+id] >> left_in[1], \
		       stop_ref[numiters*blocks+id] >> top_in[1], \
		       scenter_ref[numiters*blocks+id] >> center_in[1]) \
			output(sdfbarrier_ref[0] << token[1])
		{
		}
	} else {
		/* Block in the center */
		#pragma omp task input(sleft_ref[numiters*blocks+id] >> left_in[1], \
		       stop_ref[numiters*blocks+id] >> top_in[1], \
		       scenter_ref[numiters*blocks+id] >> center_in[1]) \
			output(sdfbarrier_ref[0] << token[1])
		{
		}
	}
}

int main(int argc, char** argv)
{
	int N = (1 << 13);
	int block_size = (1 << 8);
	int numiters = 60;

	struct timeval start;
	struct timeval end;

	int option;
	FILE* res_file = NULL;

	/* Parse options */
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
				       "  -o <output file>             Write data to output file, default is stream_seidel_from_df.out\n",
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

	if(numiters < 2) {
		fprintf(stderr, "Minimum number of iterations is 2.\n");
		exit(1);
	}

	double* matrix = malloc_interleaved(N*N*sizeof(double));

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
	int sdfbarrier[1] __attribute__((stream));

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
	sdfbarrier_ref = malloc(sizeof (void *));

	/* Copy the local table into the global one */
	memcpy (stop_ref, stop, (numiters+1)*blocks * sizeof (void *));
	memcpy (sleft_ref, sleft, (numiters+1)*blocks * sizeof (void *));
	memcpy (sright_ref, sright, (numiters+1)*blocks * sizeof (void *));
	memcpy (sbottom_ref, sbottom, (numiters+1)*blocks * sizeof (void *));
	memcpy (scenter_ref, scenter, (numiters+1)*blocks * sizeof (void *));
	memcpy (sdfbarrier_ref, sdfbarrier, sizeof (void *));

	if(res_file == NULL)
		res_file = fopen("stream_seidel_from_df.out", "w");

	/* Init matrix: M[24,24] = M[N-24,N-24] = 500.0
	 * (same data as in the sequential version */
	for (int i = 0; i < N; ++i)
		for (int j = 0; j < N; ++j)
			matrix[N*i + j] = ((i == 25 && j == 25) || (i == N-25 && j == N-25)) ? 500 : 0; //(i*7 +j*13) % 17;

	gettimeofday(&start, NULL);
	openstream_start_hardware_counters();

	/* As there are dependencies between a block's task and the same task
	 * at the next iteration, recursive task creation as in gauss_seidel_df()
	 * must have an iteration distance of at least two. However, if there
	 * are less than two iterations altogether, then only one initial task
	 * per block has to be created.
	 */

	/* Create tasks that initialize the streams and those that
	 * read the final value from the streams. The order in which
	 * The per-block tasks are created is diagonal in order to
	 * unblock tasks as soon as possible.
	 */

	int tasks_per_block = int_min(blocks_x, 4);

	for(int id_x = 0; id_x < blocks_x; id_x++) {
		for(int oid_y = 0; oid_y < blocks_x; oid_y += tasks_per_block) {
			#pragma omp task
			{
				for(int id_y = oid_y; id_y < oid_y + tasks_per_block; id_y++) {
					create_initial_task(matrix, N, numiters, block_size, id_x, id_y);
					create_next_iteration_task(matrix, blocks_x, blocks_y, block_size, numiters, 0, id_x, id_y);
				}
			}
		}
	}

	if(numiters < 2)
		for(int id_x = 0; id_x < blocks_x; id_x++)
			for(int id_y = 0; id_y < blocks_x; id_y++)
				create_terminal_task(matrix, N, numiters, block_size, id_x, id_y);

	/* Wait for all the tasks to finish */
	int dfbarrier_tokens[blocks];
	#pragma omp task input(sdfbarrier_ref[0] >> dfbarrier_tokens[blocks])
	{
	}

	#pragma omp taskwait

	openstream_pause_hardware_counters();
	gettimeofday(&end, NULL);

	printf("%.5f\n", tdiff(&end, &start));

	#if _WITH_OUTPUT
	printf("[SHM] Seidel (size %d, tile %d, iterations %d) executed in %.5f seconds\n",
		N, block_size, numiters, tdiff(&end, &start));
	dump_matrix(res_file, N, matrix);
	#endif

	#if _WITH_BINARY_OUTPUT
	dump_matrix_binary(res_file, N, matrix);
	#endif

	fclose(res_file);
	free(matrix);
	free(stop_ref);
	free(sleft_ref);
	free(sright_ref);
	free(sbottom_ref);
	free(scenter_ref);
	free(sdfbarrier_ref);

	return 0;
}
