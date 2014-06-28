/**
 * DF-reuse implementation of K-means, compatible to the non-fuzzy
 * k-means version from minebench 3.0 without z-score transformation.
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
#include <malloc.h>
#include <getopt.h>
#include <string.h>
#include "../common/common.h"
#include "../common/sync.h"
#include "kmeans_common.h"
#include "stream_df_kmeans_reuse_outline.h"

static void** sdelta_ref;
static void** sdata_ref;
static void** sccenter_in_ref;
static void** sccenter_out_ref;
static void** smembership_ref;
static void** snum_members_ref;
static void** sstop_ref;
static void** sfinal_ref;
static void** sterm_created_ref;
static void** stermdet_stop_ref;
static void** sccenter_stop_ref;

struct kmeans_round_params {
	int fan;
	int max_level;
	int max_iter;
	int num_blocks;
	int block_size;
	int k;
	int n;
	int nd;
	float* ccenters;
	float* data;
	int* mship;
	float threshold;
};

struct kmeans_round_params g_params;

int get_max_level(int num_blocks, int fan);

void create_initial_cascade_followup_tasks(struct kmeans_round_params* params, int level, int num);
void create_initial_cascade_task(struct kmeans_round_params* params, int level, int num);
void create_initial_cascade(struct kmeans_round_params* params);

void create_kmeans_task(struct kmeans_round_params* params, int iter, int block);
void create_kmeans_followup_tasks(struct kmeans_round_params* params, int iter, int block, int stop);

void create_reduction_task(struct kmeans_round_params* params, int iter, int level, int num);
void create_reduction_followup_tasks(struct kmeans_round_params* params, int iter, int level, int num);

void create_propagation_cascade_followup_tasks(struct kmeans_round_params* params, int iter, int level, int num, int stop);
void create_propagation_cascade_task(struct kmeans_round_params* params, int iter, int level, int num);
void create_propagation_cascade(struct kmeans_round_params* params, int iter);

static int num_fan_streams(int n, int fan) {
	return ((n*fan-1) / (fan-1));
}

int num_fans_done(int fan, int level, int max_level, int n)
{
	int done = 0;
	int curr = 1;

	for(int i = 0; i < level+1; i++) {
		done += curr;
		curr *= fan;
	}

	return done;
}

void create_kmeans_consumer_task(struct kmeans_round_params* params, int iter, int block)
{
	float data_in[params->block_size*params->nd];
	float cc_in[params->k*params->nd];
	int mship_in[params->block_size];
	float delta_in[1];
	int nmemb_in[params->k];
	int final_token[1];

	int fan_streams = num_fan_streams(params->num_blocks, params->fan);

	#pragma omp task input(sdata_ref[(iter+1) * params->num_blocks + block] >> data_in[params->block_size * params->nd], \
		       sccenter_out_ref[(iter+1) * fan_streams - params->num_blocks + block] >> cc_in[params->k * params->nd], \
		       smembership_ref[(iter+1) * params->num_blocks + block] >> mship_in[params->block_size], \
		       sdelta_ref[(iter+1) * fan_streams - params->num_blocks + block] >> delta_in[1], \
		       snum_members_ref[(iter+1) * fan_streams - params->num_blocks + block] >> nmemb_in[params->k]) \
		output(sfinal_ref[0] << final_token[1])
	{
	}
}

void create_propagation_cascade_followup_tasks(struct kmeans_round_params* params, int iter, int level, int num, int stop)
{
	if(level == params->max_level) {
		if(stop == 2) {
			for(int i = 0; i < params->fan; i++)
				create_kmeans_consumer_task(params, iter+1, num*params->fan+i);
		} else {
			create_reduction_task(params, iter+1, params->max_level, num);
		}
	} else if(level+2 <= params->max_level) {
		for(int i = 0; i < params->fan*params->fan; i++)
			create_propagation_cascade_task(params, iter, level+2, num*(params->fan*params->fan)+i);
	}
}

void create_propagation_cascade_task(struct kmeans_round_params* params, int iter, int level, int num)
{
	int stop_in[1][1];
	int stop_out[params->fan][1];
	float cc_out[params->fan][params->k*params->nd];
	float cc_in[1][params->k*params->nd];

	int sstop_out[params->fan] __attribute__((stream_ref));
	float sccenter_out[params->fan] __attribute__((stream_ref));

	int sstop_in[1] __attribute__((stream_ref));
	float sccenter_in[1] __attribute__((stream_ref));

	int fan_streams = num_fan_streams(params->num_blocks, params->fan);
	int done = num_fans_done(params->fan, level, params->max_level, params->num_blocks) + (iter+1) * fan_streams;
	int done_next = num_fans_done(params->fan, level-1, params->max_level, params->num_blocks) + (iter+1) * fan_streams;

	for(int i = 0; i < params->fan; i++) {
		sstop_out[i] = sstop_ref[done + num*params->fan + i];
		sccenter_out[i] = sccenter_in_ref[done + num*params->fan + i];
	}

	sstop_in[0] = sstop_ref[done_next + num];
	sccenter_in[0] = sccenter_in_ref[done_next + num];

	#pragma omp task input(sstop_in >> stop_in[1][1], \
			       sccenter_in >> cc_in[1][params->k*params->nd]) \
		output(sstop_out << stop_out[params->fan][1],		\
		       sccenter_out << cc_out[params->fan][params->k*params->nd])
	{
		create_propagation_cascade_followup_tasks(params, iter, level, num, stop_in[0][0]);

		for(int i = 0; i < params->fan; i++) {
			stop_out[i][0] = stop_in[0][0];
			memcpy(&cc_out[i][0], &cc_in[0][0], params->k*params->nd*sizeof(float));
		}
	}
}

void create_propagation_cascade(struct kmeans_round_params* params, int iter)
{
	create_propagation_cascade_task(params, iter, 0, 0);

	if(params->max_level >= 1)
		for(int i = 0; i < params->fan; i++)
			create_propagation_cascade_task(params, iter, 1, i);
}

void create_reduction_followup_tasks(struct kmeans_round_params* params, int iter, int level, int num)
{
	if(level - 2 >= 0 && (num % (params->fan*params->fan)) == 0) {
		create_reduction_task(params, iter, level-2, num / (params->fan*params->fan));
	}

	if(level == 1 && num == 0) {
		create_propagation_cascade(params, iter);
	}
}

void create_reduction_task(struct kmeans_round_params* params, int iter, int level, int num)
{
	float delta_in[params->fan][1];
	float delta_out[1][1];
	int nmemb_in[params->fan][params->k];
	int nmemb_out[1][params->k];
	float cc_in[params->fan][params->k*params->nd];
	float cc_out[1][params->k*params->nd];
	int stop_out[1][1];
	int nstop_out;
	int ndelta_out;
	int nnmemb_out;

	float sdelta_in[params->fan] __attribute__((stream_ref));
	int snum_members_in[params->fan] __attribute__((stream_ref));
	float sccenter_in[params->fan] __attribute__((stream_ref));
	float sstop_out[1] __attribute__((stream_ref));

	float sdelta_out[1] __attribute__((stream_ref));
	int snum_members_out[1] __attribute__((stream_ref));
	float sccenter_out[1] __attribute__((stream_ref));

	int fan_streams = num_fan_streams(params->num_blocks, params->fan);
	int done = num_fans_done(params->fan, level, params->max_level, params->num_blocks) + iter * fan_streams;
	int done_next = num_fans_done(params->fan, level-1, params->max_level, params->num_blocks) + iter * fan_streams;

	for(int i = 0; i < params->fan; i++) {
		sdelta_in[i] = sdelta_ref[done + num*params->fan + i];
		snum_members_in[i] = snum_members_ref[done + num*params->fan + i];
		sccenter_in[i] = sccenter_out_ref[done + num*params->fan + i];
	}

	if(level == 0) {
		sccenter_out[0] = sccenter_in_ref[(iter+1) * fan_streams];
		sstop_out[0] = sstop_ref[(iter+1) * fan_streams];

		nstop_out = 1;
		ndelta_out = 0;
		nnmemb_out = 0;
	} else {
		sdelta_out[0] = sdelta_ref[done_next + num];
		snum_members_out[0] = snum_members_ref[done_next + num];
		sccenter_out[0] = sccenter_out_ref[done_next + num];

		nstop_out = 0;
		ndelta_out = 1;
		nnmemb_out = 1;
	}

	#pragma omp task input(sdelta_in >> delta_in[params->fan][1], \
			       snum_members_in >> nmemb_in[params->fan][params->k], \
			       sccenter_in >> cc_in[params->fan][params->k*params->nd]) \
		output(sdelta_out << delta_out[ndelta_out][1], \
		       snum_members_out << nmemb_out[nnmemb_out][params->k], \
		       sccenter_out << cc_out[1][params->k*params->nd], \
		       sstop_out << stop_out[nstop_out][1])
	{
		create_reduction_followup_tasks(params, iter, level, num);

		int nmemb_out_lcl[params->k];
		float delta_out_lcl = 0.0f;

		memset(&nmemb_out_lcl, 0, params->k*sizeof(int));
		memset(&cc_out[0][0], 0, params->k*params->nd*sizeof(float));

		for(int i = 0; i < params->fan; i++) {
			delta_out_lcl += delta_in[i][0];

			for(int j = 0; j < params->k; j++) {
				nmemb_out_lcl[j] += nmemb_in[i][j];

				for(int k = 0; k < params->nd; k++)
					cc_out[0][j*params->nd+k] += cc_in[i][j*params->nd+k];
			}
		}

		delta_out_lcl /= params->fan;

		if(level == 0) {
			for(int j = 0; j < params->k; j++)
				for(int k = 0; k < params->nd; k++)
					cc_out[0][j*params->nd+k] /= nmemb_out_lcl[j];

			if(delta_out_lcl >= 0.0f)
				stop_out[0][0] = (delta_out_lcl <= params->threshold);
			else
				stop_out[0][0] = 2;
		} else {
			memcpy(&nmemb_out[0][0], nmemb_out_lcl, params->k*sizeof(int));
			delta_out[0][0] = delta_out_lcl;
		}
	}
}

void create_kmeans_followup_tasks(struct kmeans_round_params* params, int iter, int block, int stop)
{
	if(!stop) {
		create_kmeans_task(params, iter+2, block);
	}

	if(stop != 2) {
		if(block % params->fan == 0) {
			if(params->max_level >= 1) {
				if(block % (params->fan*params->fan) == 0)
					create_reduction_task(params, iter, params->max_level-1, block/(params->fan*params->fan));
			}
		}
	}

	if(block % params->fan == 0 && params->max_level == 0)
		create_propagation_cascade(params, iter);
}

void create_kmeans_task(struct kmeans_round_params* params, int iter, int block)
{
	float data[params->block_size*params->nd];
	float cc[params->k*params->nd];
	int stop_in[1];

	int mship[params->block_size];
	float delta_out[1];
	int nmemb_out[params->k];

	int fan_streams = num_fan_streams(params->num_blocks, params->fan);

	if(block == 0)
		printf("** K-means: iter %d, block %d\n", iter, block);

	#pragma omp task input(sstop_ref[(iter+1) * fan_streams - params->num_blocks + block] >> stop_in[1]) \
		output(sdelta_ref[(iter+1) * fan_streams - params->num_blocks + block] << delta_out[1], \
		       snum_members_ref[(iter+1) * fan_streams - params->num_blocks + block] << nmemb_out[params->k]) \
		inout_reuse(sdata_ref[iter * params->num_blocks + block] >> data[params->block_size * params->nd] >> sdata_ref[(iter+1) * params->num_blocks + block], \
			    smembership_ref[iter * params->num_blocks + block] >> mship[params->block_size] >> smembership_ref[(iter+1) * params->num_blocks + block], \
			    sccenter_in_ref[(iter+1) * fan_streams - params->num_blocks + block] >> cc[params->k * params->nd] >> sccenter_out_ref[(iter+1) * fan_streams - params->num_blocks + block])
	{
		float delta;

		if(!stop_in[0]) {
			if(!stop_in[0])
				delta = kmeans(params->k, params->nd, &cc[0], params->block_size, &data[0], &mship[0], &nmemb_out[0]);
			else
				delta = -1.0f;
		} else {
			delta = -1.0f;
		}

		delta_out[0] = delta;
		create_kmeans_followup_tasks(params, iter, block, stop_in[0]);
	}
}

void create_init_task(struct kmeans_round_params* params, int block)
{
	float data_out[params->block_size*params->nd];
	float cc_out[params->k*params->nd];
	int mship_out[params->block_size];
	int stop_out[1];

	int fan_streams = num_fan_streams(params->num_blocks, params->fan);

	#pragma omp task output(sstop_ref[fan_streams - params->num_blocks + block] << stop_out[1],	  \
				sdata_ref[block] << data_out[params->block_size*params->nd], \
				sccenter_in_ref[fan_streams - params->num_blocks + block] << cc_out[params->k*params->nd], \
				smembership_ref[block] << mship_out[params->block_size])
	{
		int stop = 0;

		memcpy(&stop_out[0], &stop, sizeof(int));
		memcpy(&data_out[0], &params->data[block*params->block_size*params->nd], params->block_size*params->nd*sizeof(float));
		memcpy(&cc_out[0], params->ccenters, params->k*params->nd*sizeof(float));
		memcpy(&mship_out[0], &params->mship[block*params->block_size], params->block_size*sizeof(int));
	}
}

void create_initial_cascade_followup_tasks(struct kmeans_round_params* params, int level, int num)
{
	if(level+2 <= params->max_level) {
		for(int i = 0; i < (params->fan*params->fan); i++)
			create_initial_cascade_task(params, level+2, params->fan*params->fan*num + i);
	} else if(level+2 == params->max_level+1) {
		for(int i = 0; i < (params->fan*params->fan); i++) {
			create_kmeans_task(params, 0, params->fan*params->fan*num + i);
			create_kmeans_task(params, 1, params->fan*params->fan*num + i);

			create_init_task(params, params->fan*params->fan*num + i);
		}
	}

	if(level == params->max_level-1)
		for(int i = 0; i < params->fan; i++)
			create_reduction_task(params, 0, params->max_level, num*params->fan + i);

}

void create_initial_cascade_task(struct kmeans_round_params* params, int level, int num)
{
	#pragma omp task
	{
		create_initial_cascade_followup_tasks(params, level, num);
	}
}

void create_initial_cascade(struct kmeans_round_params* params)
{
	if(params->max_level > 0) {
		create_initial_cascade_task(params, 0, 0);

		for(int i = 0; i < params->fan; i++)
			create_initial_cascade_task(params, 1, i);
	} else {
		for(int i = 0; i < params->num_blocks; i++) {
			create_kmeans_task(params, 0, i);
			create_kmeans_task(params, 1, i);
		}

		for(int i = 0; i < params->num_blocks / params->fan; i++) {
			create_reduction_task(params, 0, params->max_level, i);
		}
	}
}

int get_max_level(int num_blocks, int fan)
{
	int levels = 0;

	for(int i = num_blocks; i > fan; i /= fan)
		levels++;

	return levels;
}

struct timeval start;
struct timeval end;

int main(int argc, char** argv)
{
	int k_min = DEFAULT_K_MIN;
	int k_max = DEFAULT_K_MAX;
	g_params.nd = DEFAULT_NUM_DIMS;
	g_params.n = DEFAULT_NUM_POINTS;
	g_params.max_iter = DEFAULT_MAX_ITER;
	g_params.fan = DEFAULT_FAN;
	g_params.threshold = DEFAULT_THRESHOLD;
	g_params.data = NULL;
	int option;
	const char* in_file = NULL;
	g_params.mship = NULL;
	g_params.ccenters = NULL;
	int* nmembers;
	int i;
	int verbose = 0;
	g_params.block_size = DEFAULT_BLOCK_SIZE;
	g_params.num_blocks = g_params.n / g_params.block_size;

	/* Parse options */
	while ((option = getopt(argc, argv, "b:d:f:i:m:n:p:t:vh")) != -1)
	{
		switch(option)
		{
			case 'b':
				g_params.block_size = atoi(optarg);
				break;
			case 'd':
				g_params.nd = atoi(optarg);
				break;
			case 'f':
				g_params.fan = atoi(optarg);
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
				g_params.n = atoi(optarg);
				break;
			case 'v':
				verbose = 1;
				break;
			case 'h':
				printf("Usage: %s [option]...\n\n"
				       "Options:\n"
				       "  -b <block_size>              Block size, default is %d\n"
				       "  -d <dimensions>              Number of dimensions; only required if no input\n"
				       "                               file is specified; default is %d\n"
				       "  -f <fan>                  Number of dependencies per task for termination detection\n"
				       "                               Default is %d\n"
				       "  -i <input file>              Read data from an input file in minebench binary format\n"
				       "  -m <val>                     Maximal number of clusters, default is %d\n"
				       "  -n <val>                     Minimal number of clusters, default is %d\n"
				       "  -p <num_points>              Number of points; only required if no input\n"
				       "                               file is specified; default is %d\n"
				       "  -t <threshold>               Threshold for convergence, default is %f\n"
				       "  -v                           Verbose output; disabled by default\n",
				       argv[0], DEFAULT_BLOCK_SIZE, DEFAULT_NUM_DIMS, DEFAULT_FAN, DEFAULT_K_MAX, DEFAULT_K_MIN, DEFAULT_NUM_POINTS, DEFAULT_THRESHOLD);
				exit(0);
				break;
			case '?':
				fprintf(stderr, "Run %s -h for usage.\n", argv[0]);
				exit(1);
				break;
		}
	}

	if(in_file) {
		if(read_binary_file(in_file, &g_params.nd, &g_params.n, &g_params.data)) {
			fprintf(stderr, "Could not read file %s.\n", argv[1]);
			exit(1);
		}
	} else {
		if(verbose)
			printf("Allocating space for random %d points... ", g_params.n);

		if(!(g_params.data = malloc_interleaved(g_params.n*g_params.nd*sizeof(float)))) {
			fprintf(stderr, "Could not allocate point array.\n");
			exit(1);
		}

		if(verbose) {
			printf("done.\n");
			printf("Initializing %d random points... ", g_params.n); fflush(stdout);
		}

		init_random_points_random_walk_clust(g_params.nd, g_params.n, k_max, g_params.data);

		if(verbose)
			printf("done.\n");
	}

	if(!(g_params.mship = malloc_interleaved(g_params.n*sizeof(int)))) {
		fprintf(stderr, "Could not allocate membership array.\n");
		exit(1);
	}

	if(!(g_params.ccenters = malloc_interleaved(k_max*g_params.nd*sizeof(float)))) {
		fprintf(stderr, "Could not allocate cluster array.\n");
		exit(1);
	}

	if(!(nmembers = malloc_interleaved(k_max*g_params.nd*sizeof(int)))) {
		fprintf(stderr, "Could not allocate array for the number of points per cluster.\n");
		exit(1);
	}

	if(g_params.n % g_params.block_size != 0) {
		fprintf(stderr, "Number of points is not a multiple of the block size.\n");
		exit(1);
	}

	g_params.num_blocks = g_params.n / g_params.block_size;
	g_params.max_level = get_max_level(g_params.num_blocks, g_params.fan);

	int fan_streams = num_fan_streams(g_params.num_blocks, g_params.fan);
	int max_level = get_max_level(g_params.num_blocks, g_params.fan);

	int sfinal[2] __attribute__((stream));
	sfinal_ref = malloc(sizeof(void*));
	memcpy(sfinal_ref, sfinal, sizeof(void*));

	int sstop[g_params.max_iter*fan_streams] __attribute__((stream));
	sstop_ref = malloc(g_params.max_iter*fan_streams*sizeof(void*));
	memcpy(sstop_ref, sstop, g_params.max_iter*fan_streams*sizeof(void*));

	float sdata[g_params.max_iter*g_params.num_blocks] __attribute__((stream));
	sdata_ref = malloc(g_params.max_iter*g_params.num_blocks*sizeof(void*));
	memcpy(sdata_ref, sdata, g_params.max_iter*g_params.num_blocks*sizeof(void*));

	float sdelta[g_params.max_iter*fan_streams] __attribute__((stream));
	sdelta_ref = malloc(g_params.max_iter*fan_streams*sizeof(void*));
	memcpy(sdelta_ref, sdelta, g_params.max_iter*fan_streams*sizeof(void*));

	float snmemb[g_params.max_iter*fan_streams] __attribute__((stream));
	snum_members_ref = malloc(g_params.max_iter*fan_streams*sizeof(void*));
	memcpy(snum_members_ref, snmemb, g_params.max_iter*fan_streams*sizeof(void*));

	float sccenter_in[g_params.max_iter*fan_streams] __attribute__((stream));
	sccenter_in_ref = malloc(g_params.max_iter*fan_streams*sizeof(void*));
	memcpy(sccenter_in_ref, sccenter_in, g_params.max_iter*fan_streams*sizeof(void*));

	float sccenter_out[g_params.max_iter*fan_streams] __attribute__((stream));
	sccenter_out_ref = malloc(g_params.max_iter*fan_streams*sizeof(void*));
	memcpy(sccenter_out_ref, sccenter_out, g_params.max_iter*fan_streams*sizeof(void*));

	int smship[g_params.max_iter*g_params.num_blocks] __attribute__((stream));
	smembership_ref = malloc(g_params.max_iter*g_params.num_blocks*sizeof(void*));
	memcpy(smembership_ref, smship, g_params.max_iter*g_params.num_blocks*sizeof(void*));

	g_params.k = k_min;

	if(verbose) {
		printf("-------- Start clustering -----------\n"
		       "Dimensions: %d\n"
		       "Points: %d\n"
		       "Block size: %d\n"
		       "Min. K: %d\n"
		       "Max. K: %d\n"
		       "Max. Iterations: %d\n\n",
		       g_params.nd, g_params.n, g_params.block_size, k_min, k_max, g_params.max_iter);
	}

	init_membership(g_params.n, g_params.mship);
	init_clusters(g_params.k, g_params.nd, g_params.ccenters, g_params.n, g_params.data);

	gettimeofday(&start, NULL);
	openstream_start_hardware_counters();

	/* Use the different number of clusters */
	for(g_params.k = k_min; g_params.k <= k_max; g_params.k++) {
		create_initial_cascade(&g_params);

		int final_token[g_params.num_blocks];
		#pragma omp task input(sfinal_ref[0] >> final_token[g_params.num_blocks])
		{
		}

		#pragma omp taskwait
	}

	openstream_pause_hardware_counters();
	gettimeofday(&end, NULL);

	if(verbose)
		printf("\n");

	printf("%.5f\n", tdiff(&end, &start));

	free(sdata_ref);
	free(sstop_ref);
	free(sdelta_ref);
	free(snum_members_ref);
	free(sccenter_in_ref);
	free(sccenter_out_ref);
	free(smembership_ref);
	free(sfinal_ref);

	return 0;
}
