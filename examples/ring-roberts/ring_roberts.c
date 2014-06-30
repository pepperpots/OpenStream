/**
 * Sequential implementation of the ring-roberts benchmark (ring blur
 * + roberts edge detection), fully compatible to the Polybench
 * version from Polybench.
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
#include "ring_roberts_common.h"

void ring_blur(double* matrix, double* tmpmatrix, ssize_t N_x, ssize_t N_y, ssize_t block_size_x, ssize_t block_size_y)
{
	for(ssize_t yy = 1; yy < N_y-1; yy += block_size_y)
		for(ssize_t xx = 1; xx < N_x-1; xx += block_size_x)
			for(ssize_t y = yy; y < min(yy + block_size_y, N_y-1); y++)
				for(ssize_t x = xx; x < min(xx + block_size_x, N_x-1); x++)
					tmpmatrix[y*N_x+x] = (matrix[(y-1)*N_x+x-1] +
							      matrix[(y-1)*N_x+x] +
							      matrix[(y-1)*N_x+x+1] +
							      matrix[y*N_x+x-1] +
							      matrix[y*N_x+x] +
							      matrix[y*N_x+x+1] +
							      matrix[(y+1)*N_x+x-1] +
							      matrix[(y+1)*N_x+x] +
							      matrix[(y+1)*N_x+x+1]) / 8.0;
}

void roberts_edge(double* matrix, double* tmpmatrix, ssize_t N_x, ssize_t N_y, ssize_t block_size_x, ssize_t block_size_y)
{
	for(ssize_t yy = 1; yy < N_y-2; yy += block_size_y)
		for(ssize_t xx = 2; xx < N_x-1; xx += block_size_x)
			for(ssize_t y = yy; y < min(yy + block_size_y, N_y-2); y++)
				for(ssize_t x = xx; x < min(xx + block_size_x, N_x-1); x++)
					matrix[y*N_x+x] = (tmpmatrix[y*N_x+x] - tmpmatrix[(y+1)*N_x+x-1]) +
						(tmpmatrix[(y+1)*N_x+x] - tmpmatrix[y*N_x+x-1]);
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

	size_t matrix_size = sizeof(double)*N_y*N_x;
	double* matrix = malloc(matrix_size);
	double* tmpmatrix = malloc(matrix_size);

	init_matrix(matrix, N_x, N_y);
	memset(tmpmatrix, 0, matrix_size);

	gettimeofday(&start, NULL);

	ring_blur(matrix, tmpmatrix, N_x, N_y, block_size_x, block_size_y);
	roberts_edge(matrix, tmpmatrix, N_x, N_y, block_size_x, block_size_y);

	gettimeofday(&end, NULL);

	printf("%.5f\n", tdiff(&end, &start));

	if(res_file) {
		dump_matrix_2d(matrix, res_file, N_y, N_x);
		fclose(res_file);
	}

	free(matrix);
	free(tmpmatrix);

	return 0;
}
