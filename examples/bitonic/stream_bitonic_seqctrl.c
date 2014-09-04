/**
 * SHM implementation of Batcher's bitonic merge.
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

#include "bitonic.h"
#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <getopt.h>
#include <alloca.h>
#include "../common/common.h"
#include "../common/sync.h"

void** streams;
void** end_streams;
void** dfbarrier_stream;

/* log2 of the number of keys to sort */
long num_keys_log;
long num_keys;

/* log2 of the number of keys per block */
long block_size_log;
long block_size;

key_t* keys;

int num_par_stages;
int num_blocks;
int num_total_streams;

struct timeval start;
struct timeval end;

void create_init_task(int task_num);
void create_init_sort_task(int task_num);
void create_terminal_task(int task_num);
void create_triangle_task(int stage, int triangle_part, int box);
void create_atomic_compare_half_task(int stage, int ch_stage, int box, int level);
void create_compare_half_task(int stage, int ch_stage, int box, int level, int compare_half);

/* Compare upper and lower half in a triangular fashion:
 * top_in[i] is compared to bottom_in[num_keys-i-1].
 */
void triangle_merge(key_t* a, key_t* b, long num_keys)
{
	for(long i = 0; i < num_keys; i++)
		cmpxchg(&a[i], &b[num_keys-i-1]);
}

static void compare_halves_rec(key_t* top, key_t* bottom, long num_keys)
{
	if(num_keys < 1)
		return;

	compare_halves(top, bottom, num_keys);

	/* Compare boxes */
	for(long i = num_keys; i > 1; i /= 2) {
		for(long k = 0; k < num_keys; k += i) {
			compare_halves(&top[k], &top[k+i/2], i / 2);
			compare_halves(&bottom[k], &bottom[k+i/2], i / 2);
		}
	}
}

void create_init_task(int task_num)
{
	key_t top_out[1];

	#pragma omp task output(streams[task_num] << top_out[1])
	{
	}
}

void create_init_sort_task(int task_num)
{
	key_t top_in[1];
	key_t top_out[1];

	#pragma omp task input(streams[task_num] >> top_in[1]) \
			output(streams[task_num+num_blocks] << top_out[1])
	{
		key_t* block = &keys[task_num * block_size];
		quicksort_block(block, block_size);
	}
}

void create_terminal_task(int task_num)
{
	key_t top_in[1];
	int token[1];

	#pragma omp task input(streams[num_total_streams - num_blocks + task_num] >> top_in[1]) \
		output(dfbarrier_stream[0] << token[1])
	{
	}
}

void create_triangle_task(int stage, int triangle_part, int box)
{
	int stage_offset = num_blocks * (stage + 1) * (stage + 2) / 2;
	int num_blocks_in_box = (2 << stage);

	int box_offset = stage_offset + box * num_blocks_in_box;

	int top_in_offset = box_offset + triangle_part;
	int bottom_in_offset = box_offset + num_blocks_in_box - triangle_part - 1;

	int top_out_offset = top_in_offset + num_blocks;
	int bottom_out_offset = bottom_in_offset + num_blocks;

	key_t top_in[1];
	key_t top_out[1];
	key_t bottom_in[1];
	key_t bottom_out[1];

	#pragma omp task input(streams[top_in_offset]     >> top_in[1], \
			       streams[bottom_in_offset]  >> bottom_in[1]) \
			output(streams[top_out_offset]    << top_out[1], \
			       streams[bottom_out_offset] << bottom_out[1])
	{
		key_t* top = &keys[(top_in_offset % num_blocks) * block_size];
		key_t* bottom = &keys[(bottom_in_offset % num_blocks) * block_size];

		triangle_merge(top, bottom, block_size);
	}
}

void create_atomic_compare_half_task(int stage, int ch_stage, int box, int level)
{
	int stage_offset = num_blocks * (stage + 1) * (stage + 2) / 2;
	int num_blocks_in_box = (2 << stage);

	key_t top_in[1];
	key_t top_out[1];

	int left_in_offset = stage_offset + (ch_stage + 1) * num_blocks + box * num_blocks_in_box + level;
	int right_out_offset = left_in_offset + num_blocks;

	#pragma omp task input(streams[left_in_offset]   >> top_in[1]) \
			output(streams[right_out_offset] << top_out[1])
	{
		key_t* top = &keys[(left_in_offset % num_blocks) * block_size];

		compare_halves_rec(top, &top[block_size/2], block_size / 2);
	}
}

void create_compare_half_task(int stage, int ch_stage, int box, int level, int compare_half)
{
	int stage_offset = num_blocks * (stage + 1) * (stage + 2) / 2;
	int num_ch_stages = stage + 1;
	int num_blocks_in_box = (2 << stage);

	key_t top_in[1];
	key_t top_out[1];
	key_t bottom_in[1];
	key_t bottom_out[1];

	int top_in_offset = stage_offset + (ch_stage + 1) * num_blocks + box * num_blocks_in_box + level * (1 << (num_ch_stages - ch_stage - 1)) + compare_half;
	int top_out_offset = top_in_offset + num_blocks;

	int bottom_in_offset = top_in_offset +  (1 << (num_ch_stages - ch_stage - 2));
	int bottom_out_offset = bottom_in_offset + num_blocks;

	if(stage == 2 && ch_stage == 1 && level == 4)
		assert(0);

	#pragma omp task input(streams[top_in_offset]     >> top_in[1], \
			       streams[bottom_in_offset]  >> bottom_in[1]) \
			output(streams[top_out_offset]    << top_out[1], \
			       streams[bottom_out_offset] << bottom_out[1])
	{
		key_t* top = &keys[(top_in_offset % num_blocks) * block_size];
		key_t* bottom = &keys[(bottom_in_offset % num_blocks) * block_size];

		compare_halves(top, bottom, block_size);
	}
}

int main(int argc, char** argv)
{
	/* log2 of the number of keys to sort */
	num_keys_log = DEFAULT_NUM_KEYS_LOG;
	num_keys = (1 << num_keys_log);

	/* log2 of the number of keys per block */
	block_size_log = DEFAULT_BLOCK_SIZE_LOG;
	block_size = 1 << block_size_log;

	/* If set to 1 a final check is performed on the sorted data */
	int check = 0;

	int option;

	while ((option = getopt(argc, argv, "s:b:ch")) != -1)
	{
		switch(option)
		{
			case 's':
				num_keys_log = atol(optarg);
				num_keys = 1 << num_keys_log;
				break;
			case 'b':
				block_size_log = atol(optarg);
				block_size = 1 << block_size_log;
				break;
			case 'c':
				check = 1;
				break;
			case 'h':
				printf("Usage: %s [option]...\n\n"
				       "Options:\n"
				       "  -s <power>                   Set the number of integer keys to 1 << <power>,\n"
				       "                               default is %ld\n"
				       "  -b <power>                   Set the block size to 1 << <power>, default is %ld\n",
				       argv[0], num_keys_log, block_size_log);
				exit(0);
				break;
			case '?':
				fprintf(stderr, "Run %s -h for usage.\n", argv[0]);
				exit(1);
				break;
		}
	}

	if(num_keys_log < block_size_log) {
		fprintf(stderr,
			"The block size must be smaller or equal "
			"to the number of keys.\n");
		exit(1);
	}

	/* Sequence that will contain the input keys */
	keys = malloc_interleaved(num_keys*sizeof(key_t));

	/* Number of parallel stages (stages with an number of output elements
	 * greater than the block size
	 */
	num_par_stages = num_keys_log - block_size_log;

	/* Number of input / output blocks */
	num_blocks = num_keys / block_size;

	/* Streams between parallel stages */
	int num_inter_stage_streams = (num_par_stages + 2) * num_blocks;

	/* Streams within parallel stages */
	int num_intra_stage_streams = ((num_par_stages + 1) * num_par_stages)/2 * num_blocks;

	num_total_streams = num_inter_stage_streams + num_intra_stage_streams;

	key_t lstreams[num_total_streams] __attribute__((stream));
	streams = malloc(num_total_streams*sizeof(void*));
	memcpy(streams, lstreams, num_total_streams*sizeof(void*));

	int lend_streams[num_blocks] __attribute__((stream));
	end_streams = malloc(num_blocks*sizeof(void*));
	memcpy(end_streams, lend_streams, num_blocks*sizeof(void*));

	int ldfbarrier_stream[1] __attribute__((stream));
	dfbarrier_stream = malloc(sizeof(void*));
	memcpy(dfbarrier_stream, ldfbarrier_stream, sizeof(void*));

	/* Initialize input array with random keys */
	init_sequence(keys, num_keys);

	printf("Start sorting %ld keys...\n", num_keys);

	gettimeofday (&start, NULL);
	openstream_start_hardware_counters();

	for(int stage = 0; stage < num_par_stages; stage++) {
		if(stage == 0) {
			for(int block = 0; block < num_blocks; block++) {
				create_init_task(block);
				create_init_sort_task(block);
			}
		}

		for(int box = 0; box < (1 << (num_par_stages - stage - 1)); box++) {
			for(int triangle_part = 0; triangle_part < (1 << stage); triangle_part++) {
				create_triangle_task(stage, triangle_part, box);
			}
		}

		if(stage > 0) {
			for(int ch_stage = 0; ch_stage < stage; ch_stage++) {
				for(int box = 0; box < (1 << (num_par_stages - stage - 1)); box++) {
					for(int compare_half = 0; compare_half < (1 << (stage - ch_stage - 1)); compare_half++) {
						for(int level = 0; level < (1 << (ch_stage + 1)); level++) {
							create_compare_half_task(stage, ch_stage, box, level, compare_half);
						}
					}
				}
			}
		}

		for(int ch_stage = stage; ch_stage <= stage; ch_stage++) {
			for(int box = 0; box < (1 << (num_par_stages - stage - 1)); box++) {
				for(int level = 0; level < (1 << (ch_stage + 1)); level++) {
					create_atomic_compare_half_task(stage, ch_stage, box, level);
				}
			}
		}

		if(stage == num_par_stages-1) {
			for(int block = 0; block < num_blocks; block++) {
				create_terminal_task(block);
			}
		}
	}

	int tokens[num_blocks];
	#pragma omp task input(dfbarrier_stream[0] >> tokens[num_blocks])
	{
	}

	#pragma omp taskwait

	/* Cleanup */
	openstream_pause_hardware_counters();
	gettimeofday (&end, NULL);

	printf ("%.5f\n", tdiff (&end, &start));fflush(stdout);

	if(check) {
		assert(check_ascending(keys, num_keys));
		printf("Check: OK\n");
	}

	free(keys);
	free(streams);
	free(end_streams);
	free(dfbarrier_stream);

	return 0;
}
