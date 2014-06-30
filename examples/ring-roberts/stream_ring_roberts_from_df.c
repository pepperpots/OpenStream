/**
 * Shared memory parallel implementation of the ring-roberts benchmark
 * (ring blur + roberts edge detection), fully compatible to the
 * Polybench version from Polybench.
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
#include <sys/types.h>
#include "../common/common.h"
#include "../common/sync.h"
#include "ring_roberts_common.h"

static void** stop_ref;
static void** sright_ref;
static void** scenter_ref;
static void** surcorner_ref;
static void** sdfbarrier_ref;
static void** swriteauth_ref;

void ring_blur_tile(double* matrix, double* tmpmatrix, ssize_t N_y, ssize_t N_x, ssize_t block_size_y, ssize_t block_size_x, ssize_t id_y, ssize_t id_x)
{
	ssize_t blocks_x = N_x / block_size_x;
	ssize_t blocks_y = N_y / block_size_y;

	ssize_t min_x = (id_x == 0) ? 1 : 0;
	ssize_t max_x = (id_x == blocks_x-1) ? block_size_x-2 : block_size_x-1;
	ssize_t min_y = (id_y == 0) ? 1 : 0;
	ssize_t max_y = (id_y == blocks_y-1) ? block_size_y-2 : block_size_y-1;

	for(ssize_t y = min_y; y <= max_y; y++)
		for(ssize_t x = min_x; x <= max_x; x++)
			GLOBAL_TMP_VAL(x, y) = GLOBAL_AVG(x, y);
}

void roberts_edge_tile(double* matrix, double* tmpmatrix, ssize_t N_y, ssize_t N_x, ssize_t block_size_y, ssize_t block_size_x, ssize_t id_y, ssize_t id_x)
{
	ssize_t blocks_x = N_x / block_size_x;
	ssize_t blocks_y = N_y / block_size_y;

	ssize_t min_y = (id_y == 0) ? 1 : 0;
	ssize_t max_y = (id_y == blocks_y-1) ? block_size_y-3 : block_size_y-1;

	ssize_t min_x = (id_x == 0) ? 2 : 0;
	ssize_t max_x = (id_x == blocks_x-1) ? block_size_x-2 : block_size_x-1;

	for(ssize_t y = min_y; y <= max_y; y++) {
		for(ssize_t x = min_x; x <= max_x; x++) {
			GLOBAL_VAL(x, y) = (GLOBAL_TMP_VAL(x, y) - GLOBAL_TMP_VAL(x-1, y+1)) +
				(GLOBAL_TMP_VAL(x, y+1) - GLOBAL_TMP_VAL(x-1, y));
		}
	}
}

void create_ring_task(double* matrix, double* tmpmatrix, ssize_t N_y, ssize_t N_x, ssize_t block_size_y, ssize_t block_size_x, ssize_t id_y, ssize_t id_x)
{
	/* Arrays used to access the data from the streams */
	double top_out[1];
	double right_out[1];
	double center_out[1];

	double urcorner_out;

	ssize_t blocks_x = N_x / block_size_x;
	ssize_t blocks_y = N_y / block_size_y;

	/* block id */
	ssize_t id = id_y*blocks_x + id_x;

	/* Up to 8 write authorizations for roberts tasks */
	int writeauths_refs[8] __attribute__((stream_ref));
	int nwriteauths = 0;
	int writeauths_out[8][1];

	/* Left */
	if(id_x > 0)
		writeauths_refs[nwriteauths++] = swriteauth_ref[id-1];

	/* Right */
	if(id_x < blocks_x-1)
		writeauths_refs[nwriteauths++] = swriteauth_ref[id+1];

	/* Top */
	if(id_y > 0)
		writeauths_refs[nwriteauths++] = swriteauth_ref[id-blocks_x];

	/* Bottom */
	if(id_y < blocks_y-1)
		writeauths_refs[nwriteauths++] = swriteauth_ref[id+blocks_x];

	/* Upper left */
	if(id_x > 0 && id_y > 0)
		writeauths_refs[nwriteauths++] = swriteauth_ref[id-blocks_x-1];

	/* Upper right */
	if(id_x < blocks_x-1 && id_y > 0)
		writeauths_refs[nwriteauths++] = swriteauth_ref[id-blocks_x+1];

	/* Lower left */
	if(id_x > 0 && id_y < blocks_y-1)
		writeauths_refs[nwriteauths++] = swriteauth_ref[id+blocks_x-1];

	/* Lower right */
	if(id_x < blocks_x-1 && id_y < blocks_y-1)
		writeauths_refs[nwriteauths++] = swriteauth_ref[id+blocks_x+1];

	if(id_y == 0 && id_x < blocks_x-1) {
		/* Upper row, not in the right column */
		#pragma omp task output(sright_ref [id] << right_out[1], \
					scenter_ref[id] << center_out[1], \
					writeauths_refs << writeauths_out[nwriteauths][1])
		{
			ring_blur_tile(matrix, tmpmatrix, N_y, N_x, block_size_y, block_size_x, id_y, id_x);
		}
	} else if(id_y == 0 && id_x == blocks_x-1) {
		/* Upper right */
		#pragma omp task output(scenter_ref[id] << center_out[1], \
					writeauths_refs << writeauths_out[nwriteauths][1])
		{
			ring_blur_tile(matrix, tmpmatrix, N_y, N_x, block_size_y, block_size_x, id_y, id_x);
		}
	} else if(id_y > 0 && id_x < blocks_x-1) {
		/* Not in the right column, not in the top row */
		#pragma omp task output(sright_ref [id] << right_out[1], \
					stop_ref [id] << top_out[1], \
					scenter_ref[id] << center_out[1], \
					surcorner_ref[id] << urcorner_out, \
					writeauths_refs << writeauths_out[nwriteauths][1])
		{
			ring_blur_tile(matrix, tmpmatrix, N_y, N_x, block_size_y, block_size_x, id_y, id_x);
		}
	} else if (id_y > 0 && id_x == blocks_x-1) {
		/* right column, not in the top row */
		#pragma omp task output(stop_ref [id] << top_out[1], \
					scenter_ref[id] << center_out[1], \
					writeauths_refs << writeauths_out[nwriteauths][1])
		{
			ring_blur_tile(matrix, tmpmatrix, N_y, N_x, block_size_y, block_size_x, id_y, id_x);
		}
	}
}

void create_roberts_task(double* matrix, double* tmpmatrix, ssize_t N_y, ssize_t N_x, ssize_t block_size_y, ssize_t block_size_x, ssize_t id_y, ssize_t id_x)
{
	/* Arrays used to access the data from the streams */
	double bottom_in[1];
	double left_in[1];
	double center_in[1];

	double llcorner_in;

	int nwriteauth_tokens = 0;
	int barrier_token;

	ssize_t blocks_x = N_x / block_size_x;
	ssize_t blocks_y = N_y / block_size_y;

	/* block id */
	ssize_t id = id_y*blocks_x + id_x;

	/* Left */
	if(id_x > 0)
		nwriteauth_tokens++;

	/* right */
	if(id_x < blocks_x-1)
		nwriteauth_tokens++;

	/* Top */
	if(id_y > 0)
		nwriteauth_tokens++;

	/* Bottom */
	if(id_y < blocks_y-1)
		nwriteauth_tokens++;

	/* Upper left */
	if(id_x > 0 && id_y > 0)
		nwriteauth_tokens++;

	/* Upper right */
	if(id_x < blocks_x-1 && id_y > 0)
		nwriteauth_tokens++;

	/* Lower left */
	if(id_x > 0 && id_y < blocks_y-1)
		nwriteauth_tokens++;

	/* Lower right */
	if(id_x < blocks_x-1 && id_y < blocks_y-1)
		nwriteauth_tokens++;

	int writeauth_tokens[nwriteauth_tokens];

	if(id_y == blocks_y-1 && id_x == 0) {
		/* Lower left corner */
		#pragma omp task input(scenter_ref[id] >> center_in[1], \
				       swriteauth_ref[id] >> writeauth_tokens[nwriteauth_tokens]) \
			output(sdfbarrier_ref[0] << barrier_token)
		{
			roberts_edge_tile(matrix, tmpmatrix, N_y, N_x, block_size_y, block_size_x, id_y, id_x);
		}
	} else if(id_y == blocks_y-1 && id_x > 0) {
		/* Lower row, not in lower left corner  */
		#pragma omp task input(scenter_ref[id] >> center_in[1], \
				       sright_ref[id-1] >> left_in[1], \
				       swriteauth_ref[id] >> writeauth_tokens[nwriteauth_tokens]) \
			output(sdfbarrier_ref[0] << barrier_token)
		{
			roberts_edge_tile(matrix, tmpmatrix, N_y, N_x, block_size_y, block_size_x, id_y, id_x);
		}
	} else if(id_y < blocks_y-1 && id_x == 0) {
		/* left column */
		#pragma omp task input(scenter_ref[id] >> center_in[1], \
				       stop_ref[id+blocks_x] >> bottom_in[1], \
				       swriteauth_ref[id] >> writeauth_tokens[nwriteauth_tokens]) \
			output(sdfbarrier_ref[0] << barrier_token)
		{
			roberts_edge_tile(matrix, tmpmatrix, N_y, N_x, block_size_y, block_size_x, id_y, id_x);
		}
	} else if(id_x > 0 && id_y < blocks_y-1) {
		/* Remaining tiles */
		#pragma omp task input(scenter_ref[id] >> center_in[1], \
				       sright_ref[id-1] >> left_in[1], \
				       stop_ref[id+blocks_x] >> bottom_in[1], \
				       surcorner_ref[id+blocks_x-1] >> llcorner_in, \
				       swriteauth_ref[id] >> writeauth_tokens[nwriteauth_tokens]) \
			output(sdfbarrier_ref[0] << barrier_token)
		{
			roberts_edge_tile(matrix, tmpmatrix, N_y, N_x, block_size_y, block_size_x, id_y, id_x);
		}
	}
}

int main(int argc, char** argv)
{
	ssize_t N_y = -1;
	ssize_t N_x = -1;

	ssize_t block_size_y = -1;
	ssize_t block_size_x = -1;

	struct timeval start;
	struct timeval end;

	int option;
	FILE* res_file = NULL;

	/* Parse options */
	while ((option = getopt(argc, argv, "n:m:s:b:o:h")) != -1)
	{
		switch(option)
		{
			case 'n':
				if(optarg[0] == 'y')
					N_y = atoi(optarg+1);
				if(optarg[0] == 'x')
					N_x = atoi(optarg+1);
				break;
			case 's':
				if(optarg[0] == 'y')
					N_y = 1 << atoi(optarg+1);
				if(optarg[0] == 'x')
					N_x = 1 << atoi(optarg+1);
				break;
			case 'b':
				if(optarg[0] == 'y')
					block_size_y = 1 << atoi (optarg+1);
				if(optarg[0] == 'x')
					block_size_x = 1 << atoi (optarg+1);
				break;
			case 'm':
				if(optarg[0] == 'y')
					block_size_y = atoi(optarg+1);
				if(optarg[0] == 'x')
					block_size_x = atoi(optarg+1);
				break;
			case 'o':
				res_file = fopen(optarg, "w");
				break;
			case 'h':
				printf("Usage: %s [option]...\n\n"
				       "Options:\n"
				       "  -n y<size>                   Number of elements in y-direction of the matrix\n"
				       "  -n x<size>                   Number of elements in x-direction of the matrix\n"
				       "  -s y<power>                  Set number of elements in y-direction of the matrix to 1 << <power>\n"
				       "  -s x<power>                  Set number of elements in x-direction of the matrix to 1 << <power>\n"
				       "  -m y<size>                   Number of elements in y-direction of a block\n"
				       "  -m x<size>                   Number of elements in x-direction of a block\n"
				       "  -b y<block size power>       Set the block size in y-direction to 1 << <block size power>\n"
				       "  -b x<block size power>       Set the block size in x-direction to 1 << <block size power>\n"
				       "  -r <iterations>              Number of iterations\n"
				       "  -o <output file>             Write data to output file, default is stream_df_jacobi_2d.out\n",
				       argv[0]);
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

	if(N_y == -1) {
		fprintf(stderr, "Please set the size of the matrix in y-direction (using the -n or -s switch).\n");
		exit(1);
	}

	if(N_x == -1) {
		fprintf(stderr, "Please set the size of the matrix in x-direction (using the -n or -s switch).\n");
		exit(1);
	}

	if(block_size_y == -1) {
		fprintf(stderr, "Please set the block size of the matrix in y-direction (using the -m or -b switch).\n");
		exit(1);
	}

	if(block_size_x == -1) {
		fprintf(stderr, "Please set the block size of the matrix in x-direction (using the -m or -b switch).\n");
		exit(1);
	}

	if(N_y % block_size_y != 0) {
		fprintf(stderr, "Block size in y-direction (%zd) does not divide size of the matrix in y-direction (%zd).\n", block_size_y, N_y);
		exit(1);
	}

	if(N_x % block_size_x != 0) {
		fprintf(stderr, "Block size in x-direction (%zd) does not divide size of the matrix in x-direction (%zd).\n", block_size_x, N_x);
		exit(1);
	}

	ssize_t matrix_size = sizeof(double)*N_y*N_x;
	double* matrix = malloc_interleaved(matrix_size);
	double* tmpmatrix = malloc_interleaved(matrix_size);

	ssize_t blocks_y = N_y / block_size_y;
	ssize_t blocks_x = N_x / block_size_x;

	ssize_t blocks = 1 * blocks_y * blocks_x;

	int sdfbarrier[2] __attribute__((stream));
	sdfbarrier_ref = malloc(2*sizeof (void *));
	memcpy (sdfbarrier_ref, sdfbarrier, 2*sizeof (void *));

	double scenter[blocks] __attribute__((stream));
	scenter_ref = malloc(blocks * sizeof (void *));
	memcpy (scenter_ref, scenter, blocks * sizeof (void *));

	double surcorner[blocks] __attribute__((stream));
	surcorner_ref = malloc(blocks * sizeof (void *));
	memcpy (surcorner_ref, surcorner, blocks * sizeof (void *));

	double stop[blocks] __attribute__((stream));
	stop_ref = malloc(blocks * sizeof (void *));
	memcpy (stop_ref, stop, blocks * sizeof (void *));

	double sright[blocks] __attribute__((stream));
	sright_ref = malloc(blocks * sizeof (void *));
	memcpy (sright_ref, sright, blocks * sizeof (void *));

	int swriteauth[blocks] __attribute__((stream));
	swriteauth_ref = malloc(blocks * sizeof (void *));
	memcpy (swriteauth_ref, swriteauth, blocks * sizeof (void *));

	init_matrix(matrix, N_x, N_y);
	memset(tmpmatrix, 0, matrix_size);

	gettimeofday(&start, NULL);
	openstream_start_hardware_counters();

	ssize_t tasks_per_block_x = min(blocks_x, 4);
	ssize_t tasks_per_block_y = min(blocks_y, 4);

	for(ssize_t id_yy = 0; id_yy < blocks_y; id_yy += tasks_per_block_y) {
		for(ssize_t id_xx = 0; id_xx < blocks_x; id_xx += tasks_per_block_y) {
			#pragma omp task
			{
				for(ssize_t id_y = id_yy; id_y < min(id_yy + tasks_per_block_y, blocks_y); id_y++) {
					for(ssize_t id_x = id_xx; id_x < min(id_xx + tasks_per_block_x, blocks_x); id_x++) {
						create_ring_task(matrix, tmpmatrix, N_y, N_x, block_size_y, block_size_x, id_y, id_x);
						create_roberts_task(matrix, tmpmatrix, N_y, N_x, block_size_y, block_size_x, id_y, id_x);
					}
				}
			}
		}
	}

	/* Wait for all the tasks to finish */
	int dfbarrier_tokens[blocks];
	#pragma omp task input(sdfbarrier_ref[0] >> dfbarrier_tokens[blocks])
	{
	}

	#pragma omp taskwait

	openstream_pause_hardware_counters();
	gettimeofday(&end, NULL);

	printf("%.5f\n", tdiff(&end, &start));

	if(res_file) {
		dump_matrix_2d(matrix, res_file, N_y, N_x);
		fclose(res_file);
	}

	free(matrix);
	free(tmpmatrix);
	free(scenter_ref);
	free(sdfbarrier_ref);
	free(stop_ref);
	free(sright_ref);
	free(surcorner_ref);
	free(swriteauth_ref);

	return 0;
}
