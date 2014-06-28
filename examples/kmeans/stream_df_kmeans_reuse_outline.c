#include <string.h>
#include <float.h>
#include "kmeans_common.h"

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
float kmeans(int k, int nd, float* ccenters, int n, float* vals, int* membership, int* cluster_memb)
{
	float min_dist;
	int min_cid = 0;
	float delta = 0.0f;
	float curr_dists[k];

	memset(cluster_memb, 0, k*sizeof(int));

	/* Determine nearest cluster for each point */
	for(int i = 0; i < n; i++) {
		min_dist = FLT_MAX;

		/* Calculate distance for each cluster center and
		 * find id of the nearest cluster
		 */
		for(int cluster = 0; cluster < k; cluster++)
			curr_dists[cluster] = euclid_dist_sq(nd, &ccenters[cluster*nd], &vals[i*nd]);

		for(int cluster = 0; cluster < k; cluster++) {
			if(curr_dists[cluster] < min_dist) {
				min_dist = curr_dists[cluster];
				min_cid = cluster;
			}
		}

		/* Update membership */
		if(membership[i] == -1 || membership[i] != min_cid) {
			membership[i] = min_cid;
			delta += 1.0f;
		}
	}

	memset(ccenters, 0, k*nd*sizeof(float));

	for(int i = 0; i < n; i++) {
		cluster_memb[membership[i]]++;

		for(int dim = 0; dim < nd; dim++)
			ccenters[membership[i]*nd + dim] += vals[i*nd+dim];
	}

	delta = delta / n;

	return delta;
}
