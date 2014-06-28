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

#include "kmeans_common.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

/**
 * Returns a random float value in [-1.0;1.0]
 */
float frand(void)
{
	return (float)(rand() - RAND_MAX/2) / (float)RAND_MAX;
}

float frand_from_int(int* rands)
{
	*rands = (*rands) * 1103515245 + 12345;
	int nbits = sizeof(*rands)*8;
	int rnd = (*rands) >> (nbits/2);
	double d = rnd / ((double)(1 << (nbits / 2)));

	return d;
}

/**
 * Initializes the membership array with -1
 *
 * n: number of points
 * membership: array for memberships
 */
void init_membership(int n, int* membership)
{
	for(int i = 0; i < n; i++)
		membership[i] = -1;
}

/**
 * Initialize the cluster centers by choosing members
 * from the set of points randomly.
 *
 * k: number of clusters
 * nd: number of dimensions
 * ccenters: array that contains the initial coordinates after the call
 */
void init_clusters(int k, int nd, float* ccenters, int n, float* vals)
{
	srand(799292);
	for(int i = 0; i < k; i++) {
		/* Pick a random point */
		int pt = rand() % n;

		/* Copy the points' coordinates */
		for(int dim = 0; dim < nd; dim++)
			ccenters[i*nd+dim] = vals[pt*nd + dim];
	}
}

/**
 * Read the contents of a file in minebench binary format.
 *
 * filename: full path to the file to be loaded
 * nd: contains the number of dimensions after the call
 * n: contains the number of points after the call
 * vals: contains a pointer to the point data; the array
 *       is allocated automatically.
 *
 * Returns 0 if the file could be read correctly, otherwise 1
 */
int read_binary_file(const char* filename, int* nd, int* n, float** vals)
{
	FILE* fp = fopen(filename, "rb");
	int ret = 1;

	if(!fp)
		goto out;

	/* Read number of points */
	if(fread(n, sizeof(*n), 1, fp) != 1)
		goto out_fp;

	/* Read number of dimensions */
	if(fread(nd, sizeof(*nd), 1, fp) != 1)
		goto out_fp;

	/* Allocate point array */
	if(!(*vals = malloc((*n) * (*nd) * sizeof(float))))
		goto out_fp;

	/* Read point data */
	if(fread(*vals, (*n) * (*nd) * sizeof(float), 1, fp) != 1) {
		free(*vals);
		goto out_fp;
	}

	ret = 0;

out_fp:
	fclose(fp);
out:
	return ret;
}

/**
 * Initializes an array of points with random coordinates.
 * For each cluster a random point is generated and n/k
 * iterations of a random walk are performed.
 *
 * nd: number of dimensions
 * n: number of points
 * k: number of clusters
 * vals: previously allocated array for point data
 */
void init_random_points_random_walk_clust(int nd, int n, int k, float* vals)
{
	int rands = 77777 + k * 19;
	int vals_per_clust = n / k;
	float ccenter[nd];
	float walk[nd];

	srand(226882);

	for(int i = 0; i < n; i++) {
		if(i % vals_per_clust == 0) {
			memset(walk, 0, sizeof(walk));
			for(int dim = 0; dim < nd; dim++)
				ccenter[dim] = frand_from_int(&rands);
		}

		for(int dim = 0; dim < nd; dim++) {
			walk[dim] += frand_from_int(&rands);
			float val = walk[dim];
			vals[i*nd+dim] = ccenter[dim] + val;
		}
	}
}
