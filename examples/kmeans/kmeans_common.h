/**
 * Code commonly used in several versions of k-means.
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

#define DEFAULT_K_MIN 4
#define DEFAULT_K_MAX 13
#define DEFAULT_NUM_DIMS 3
#define DEFAULT_NUM_POINTS 10000
#define DEFAULT_MAX_ITER 100
#define DEFAULT_THRESHOLD 0.001
#define DEFAULT_BLOCK_SIZE (1 << 18)
#define DEFAULT_FAN 8

float frand(void);
void init_membership(int n, int* membership);
void init_clusters(int k, int nd, float* ccenters, int n, float* vals);
int read_binary_file(const char* filename, int* nd, int* n, float** vals);
void init_random_points_random_walk_clust(int nd, int n, int k, float* vals);

/**
 * Returns the square value of the euclidian distance between
 * two n-dimensional points ref and val
 */
static inline float euclid_dist_sq(int n, float* ref, float* val)
{
	float dist = 0.0f;
	float curr;

	for(int i = 0; i < n; i++) {
		curr = val[i] - ref[i];
		dist += curr * curr;
	}

	return dist;
}

static inline int dump_kmeans_gnuplot(const char* out_file, int nd, int n, int k, float* vals, int* membership)
{
	FILE* fp = fopen(out_file, "w+");

	if(!fp)
		return 1;

	for(int i = 0; i < n; i++) {
		for(int j = 0; j < nd; j++)
			fprintf(fp, "%f ", vals[i*nd+j]);

		fprintf(fp, "%d\n", membership[i]);
	}

	return 0;
}
