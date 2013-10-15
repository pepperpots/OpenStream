/**
 * Serial implementation of K-means, compatible to the non-fuzzy
 * k-means version from minebench 3.0 without z-score transformation.
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
#include <float.h>
#include <malloc.h>
#include <getopt.h>
#include <string.h>
#include "../common/common.h"
#include "kmeans_common.h"

/**
 * Calculates the new cluster centers based on the new memberships
 *
 * k: number of clusters
 * nd: number of dimensions
 * ccenters: array that contains the updated coordinates after the call
 * nmembers: stores how many members each cluster has
 * n: number of points
 * vals: the points
 * membership: contains the ids of the clusters the points are currently
 *             associated to
 */
void update_clusters(int k, int nd, float* ccenters, int* nmembers, int n, float* vals, int* membership)
{
	/* Reset cluster centers and number of members per cluster */
	memset(ccenters, 0, sizeof(float)*k*nd);
	memset(nmembers, 0, sizeof(int)*k);

	/* Accumulate coordinates and count members */
	for(int i = 0; i < n; i++) {
		for(int dim = 0; dim < nd; dim++)
			ccenters[membership[i] * nd + dim] += vals[i*nd + dim];

		nmembers[membership[i]]++;
	}

	/* Normalize coordinates */
	for(int clust = 0; clust < k; clust++)
		for(int dim = 0; dim < nd; dim++)
			ccenters[clust*nd + dim] /= nmembers[clust];
}

/**
 * Performs one single iteration of the k-means algorithm.
 *
 * k: number of clusters
 * nd: number of dimensions
 * ccenters: current cluster centers
 * n: number of points
 * vals: the points to be associated to the clusters
 * membership: contains the ids of the clusters the points are currently
 *             associated to. Contains the new cluster ids after the call
 *
 * Returns the fraction of points for which the id of the cluster changed.
 */
float kmeans(int k, int nd, float* ccenters, int n, float* vals, int* membership)
{
	float min_dist;
	float curr_dist;
	int min_cid = 0;
	float delta = 0.0f;

	/* Determine nearest cluster for each point */
	for(int i = 0; i < n; i++) {
		min_dist = FLT_MAX;

		/* Calculate distance for each cluster center and
		 * find id of the nearest cluster
		 */
		for(int cluster = 0; cluster < k; cluster++) {
			curr_dist = euclid_dist_sq(nd, &ccenters[cluster*nd], &vals[i*nd]);

			if(curr_dist < min_dist) {
				min_dist = curr_dist;
				min_cid = cluster;
			}
		}

		/* Update membership */
		if(membership[i] == -1 || membership[i] != min_cid) {
			membership[i] = min_cid;
			delta += 1.0f;
		}
	}

	delta = delta / n;

	return delta;
}

int main(int argc, char** argv)
{
	int k_min = DEFAULT_K_MIN;
	int k_max = DEFAULT_K_MAX;
	int nd = DEFAULT_NUM_DIMS;
	int n = DEFAULT_NUM_POINTS;
	int niter = DEFAULT_MAX_ITER;
	float threshold = DEFAULT_THRESHOLD;
	float delta;
	float* vals;
	int option;
	const char* in_file = NULL;
	int* membership;
	float* cluster_centers;
	int* nmembers;
	int i;
	int verbose = 0;

	struct timeval start;
	struct timeval end;

	/* Parse options */
	while ((option = getopt(argc, argv, "d:i:m:n:p:t:vh")) != -1)
	{
		switch(option)
		{
			case 'd':
				nd = atoi(optarg);
				break;
			case 'i':
				in_file = optarg;
				break;
			case 'm':
				k_max = atoi(optarg);
				break;
			case 'n':
				k_min = atoi(optarg);
				break;
			case 'p':
				n = atoi(optarg);
				break;
			case 'v':
				verbose = 1;
				break;
			case 'h':
				printf("Usage: %s [option]...\n\n"
				       "Options:\n"
				       "  -d <dimensions>              Number of dimensions; only required if no input\n"
				       "                               file is specified; default is %d\n"
				       "  -i <input file>              Read data from an input file in minebench binary format\n"
				       "  -m <val>                     Maximal number of clusters, default is %d\n"
				       "  -n <val>                     Minimal number of clusters, default is %d\n"
				       "  -p <num_points>              Number of points; only required if no input\n"
				       "                               file is specified; default is %d\n"
				       "  -t <threshold>               Threshold for convergence, default is %f\n"
				       "  -v                           Verbose output; disabled by default\n",
				       argv[0], DEFAULT_NUM_DIMS, DEFAULT_K_MAX, DEFAULT_K_MIN, DEFAULT_NUM_POINTS, DEFAULT_THRESHOLD);
				exit(0);
				break;
			case '?':
				fprintf(stderr, "Run %s -h for usage.\n", argv[0]);
				exit(1);
				break;
		}
	}

	if(in_file) {
		if(read_binary_file(in_file, &nd, &n, &vals)) {
			fprintf(stderr, "Could not read file %s.\n", argv[1]);
			exit(1);
		}
	} else {
		if(!(vals = malloc(n*nd*sizeof(float)))) {
			fprintf(stderr, "Could not allocate point array.\n");
			exit(1);
		}

		init_random_points_random_walk_clust(nd, n, k_max, vals);
	}

	if(!(membership = malloc(n*nd*sizeof(int)))) {
		fprintf(stderr, "Could not allocate membership array.\n");
		exit(1);
	}

	if(!(cluster_centers = malloc(k_max*nd*sizeof(float)))) {
		fprintf(stderr, "Could not allocate cluster array.\n");
		exit(1);
	}

	if(!(nmembers = malloc(k_max*nd*sizeof(int)))) {
		fprintf(stderr, "Could not allocate array for the number of points per cluster.\n");
		exit(1);
	}

	if(verbose) {
		printf("-------- Start clustering -----------\n"
		       "Dimensions: %d\n"
		       "Points: %d\n"
		       "Min. K: %d\n"
		       "Max. K: %d\n"
		       "Max. Iterations: %d\n\n",
		       nd, n, k_min, k_max, niter);
	}

	gettimeofday(&start, NULL);

	/* Use the different number of clusters */
	for(int k = k_min; k <= k_max; k++) {
		/* Reset arrays and delta */
		delta = threshold+1.0f;
		init_membership(n, membership);
		init_clusters(k, nd, cluster_centers, n, vals);

		/* Perform at most niter iterations or break on convergence */
		for(i = 0; i < niter && delta > threshold; i++) {
			delta = kmeans(k, nd, cluster_centers, n, vals, membership);
			update_clusters(k, nd, cluster_centers, nmembers, n, vals, membership);
		}

		if(verbose)
			printf("K = %d, iterations = %d, final delta = %f\n", k, i-1, delta);
	}

	gettimeofday(&end, NULL);

	if(verbose)
		printf("\n");

	printf("%.5f\n", tdiff(&end, &start));

	/* Cleanup arrays */
	free(vals);
	free(membership);
	free(cluster_centers);
	free(nmembers);

	return 0;
}
