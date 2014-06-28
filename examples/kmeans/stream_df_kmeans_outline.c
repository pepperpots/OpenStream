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
float kmeans(int k, int nd, float* ccenters_in, float* ccenters_out, int n, float* vals, int* membership, int* cluster_memb, float* vals_copy_out)
{
	float min_dist;
	float curr_dists[k];
	int min_cid = 0;
	float delta = 0.0f;

	memset(ccenters_out, 0, k*nd*sizeof(float));
	memset(cluster_memb, 0, k*sizeof(int));

	/* Determine nearest cluster for each point */
	for(int i = 0; i < n; i++) {
		min_dist = FLT_MAX;

		/* Calculate distance for each cluster center and
		 * find id of the nearest cluster
		 */
		for(int cluster = 0; cluster < k; cluster++)
			curr_dists[cluster] = euclid_dist_sq(nd, &ccenters_in[cluster*nd], &vals[i*nd]);

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

		for(int d = 0; d < nd; d++)
			vals_copy_out[i*nd+d] = vals[i*nd+d];
	}

	for(int i = 0; i < n; i++) {
		cluster_memb[membership[i]]++;

		for(int dim = 0; dim < nd; dim++)
			ccenters_out[membership[i]*nd + dim] += vals[i*nd+dim];
	}

	delta = delta / n;

	return delta;
}
