/**
 * Dataflow implementation of Batcher's bitonic merge.
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
#include "../common/common.h"
#include "../common/sync.h"

/* Streams within parallel stages */
void** intra_streams;

/* Streams between parallel stages */
void** inter_streams;

/* Stream used for synchornization with the terminating tasks */
void** end_stream;

struct timeval start;
struct timeval end;
struct profiler_sync sync;

/* Compare upper and lower half in a triangular fashion:
 * top_in[i] is compared to bottom_in[num_keys-i-1].
 * The result is written to top_out and bottom_out using
 * the same indexes.
 */
static inline void triangle_merge_inout(key_t* top_in, key_t* bottom_in,
					key_t* top_out, key_t* bottom_out,
					long num_keys)
{
	for(long i = 0; i < num_keys; i++) {
		cmpxchg_inout(&top_in[i], &bottom_in[num_keys-i-1],
			      &top_out[i], &bottom_out[num_keys-i-1]);
	}
}

/* Compare top_in[i] and bottom_in[i].
 * The result is written to top_out and bottom_out using
 * the same indexes.
 */
static inline void compare_halves_inout(key_t* top_in, key_t* bottom_in,
					key_t* top_out, key_t* bottom_out,
					long num_keys)
{
	for(long i = 0; i < num_keys; i++) {
		cmpxchg_inout(&top_in[i], &bottom_in[i],
			      &top_out[i], &bottom_out[i]);
	}
}

/* Compare top_in[i] and bottom_in[i]. Afterwards the top and the  bottom keys are
 * split into equal parts and the same procedure is repeated for the subsets.
 * The final values are written to top_out and bottom_out using the same indexes
 * as for top_in and bottom_in.
 */
static inline void compare_halves_rec(key_t* top_in, key_t* bottom_in,
				      key_t* top_out, key_t* bottom_out,
				      long num_keys)
{
	/* Compare top and bottom */
	compare_halves(top_in, bottom_in, num_keys);

	/* At each iteration split keys into two equal parts and repeat the
	 * procedure for both parts independently
	 */
	for(long level = num_keys; level > 1; level /= 2) {
		if(level == 2) {
			/* Final level, 2 top keys and 2 bottom keys are left.
			 * The result of the comparison whould go into top_out
			 * and bottom_out, so use compare_halves_inout() instead
			 * of compare_halves(). */
			for(long i = 0; i < num_keys; i += level) {
				compare_halves_inout(&top_in[i],
						     &top_in[i+level/2],
						     &top_out[i],
						     &top_out[i+level/2],
						     1);
				compare_halves_inout(&bottom_in[i],
						     &bottom_in[i+level/2],
						     &bottom_out[i],
						     &bottom_out[i+level/2],
						     1);
			}
		} else {
			/* At all other levels perform in-place comparisons */
			for(long i = 0; i < num_keys; i += level) {
				compare_halves(&top_in[i], &top_in[i+level/2],
					       level/2);
				compare_halves(&bottom_in[i], &bottom_in[i+level/2],
					       level/2);
			}
		}
	}
}

void stream_compare_halves(long num_keys, long block_size,
			       void** sin_top, void** sout_top,
			       void** sin_bottom, void** sout_bottom)
{
	key_t top_in[block_size];
	key_t top_out[block_size];
	key_t bottom_in[block_size];
	key_t bottom_out[block_size];

	if(num_keys == block_size) {
		#pragma omp task input(sin_top[0] >> top_in[block_size], \
					sin_bottom[0] >> bottom_in[block_size]) \
				output(sout_top[0] << top_out[block_size], \
					sout_bottom[0] << bottom_out[block_size])
		{
			compare_halves_inout(top_in, bottom_in,
					     top_out, bottom_out,
					     block_size);
		}
	} else if(num_keys == block_size / 2) {
		#pragma omp task input(sin_top[0] >> top_in[block_size]) \
				output(sout_top[0] << top_out[block_size])
		{
			compare_halves_rec(&top_in[0], &top_in[block_size/2],
					   &top_out[0], &top_out[block_size/2],
					   block_size / 2);
		}
	}
}

/* Create tasks that compare an upper and a lower half in a triangular fashion.
 * sin_top specifies the streams containing the blocks of the top part,
 * sin_bottom those that contain the bottom parts.
 *
 * The result is written to the streams in sout_top and sout_bottom using
 * the same indexes as for the input streams.
 */
void stream_triangle_merge(long num_keys, long block_size,
			   void** sin_top, void** sout_top,
			   void** sin_bottom, void** sout_bottom)
{
	long num_blocks = num_keys / block_size;
	key_t top_in[block_size];
	key_t top_out[block_size];
	key_t bottom_in[block_size];
	key_t bottom_out[block_size];

	for(long i = 0; i < num_blocks / 2; i++) {
		#pragma omp task input(sin_top[i] >> top_in[block_size], \
				       sin_bottom[num_blocks / 2 - i - 1] >> bottom_in[block_size]) \
				output(sout_top[i] << top_out[block_size], \
				       sout_bottom[num_blocks / 2 - i - 1] << bottom_out[block_size])
		{
			triangle_merge_inout(top_in, bottom_in, top_out, bottom_out, block_size);
		}
	}
}

/* Recursively generate tasks for sorting boxes. A sorting box for a sequence
 * of keys contains a triangle merge followed by top-bottom comparisons with a
 * decreasing number of input lines.
 *
 * At each step of the recursion, the nulber of keys is divided by two. If it
 * is equal to the block size, then a merge sort instead of a bitonic merge
 * is performed.
 */
void stream_sort(long num_keys_log, long block_size_log,
		 void** inter_streams,
		 void** intra_streams,
		 long num_inter_streams_total,
		 long num_intra_streams_total)
{
	long num_keys = (1 << num_keys_log);
	long block_size = (1 << block_size_log);

	long num_blocks = (1 << (num_keys_log - block_size_log));
	key_t top_in[block_size];
	key_t top_out[block_size];

	/* Number of output streams of this step */
	long num_inter_streams = num_blocks;

	if(num_blocks == 1) {
		/* If only one block is left, then perform a merge sort */
		#pragma omp task input(inter_streams[1] >> top_in[block_size]) \
			output(inter_streams[0] << top_out[block_size])
		{
			mergesort_block(top_in, top_out, block_size);
		}

		return;
	}

	/* At least two blocks */
	long num_intra_streams = (num_keys_log - block_size_log) * num_blocks;

	long num_inter_streams_following = num_inter_streams_total - num_blocks;
	long num_intra_streams_following = num_intra_streams_total - num_intra_streams;

	/* Create tasks that sort the top part */
	stream_sort(num_keys_log - 1, block_size_log,
		    &inter_streams[num_inter_streams],
		    &intra_streams[num_intra_streams],
		    num_inter_streams_following / 2,
		    num_intra_streams_following / 2);

	/* Create tasks that merge the two parts */
	stream_triangle_merge(num_keys, block_size,
		     &inter_streams[num_inter_streams],
		     &intra_streams[0],
		     &inter_streams[num_inter_streams + num_inter_streams_following/2],
		     &intra_streams[num_blocks/2]);

	/* Create tasks that sort the bottom part */
	stream_sort(num_keys_log - 1, block_size_log,
		    &inter_streams[num_inter_streams + num_inter_streams_following/2],
		    &intra_streams[num_intra_streams + num_intra_streams_following/2],
		    num_inter_streams_following / 2,
		    num_intra_streams_following / 2);

	/* Create compare boxes */
	int max_level = num_keys_log - block_size_log;

	/* The dependence graph for the compare boxes has a tree-like structure,
	 * the number of boxes per level increases, while the number of dependeces
	 * decreases
	 */
	for(long level = 0; level < max_level; level++) {
		int blocks_per_compare_box = 1 << (num_keys_log - block_size_log - level - 1);

		/* Base offset in the current intra-stream table for this level */
		int base_offset = level*num_blocks;

		for(int compare_box = 0; compare_box < (1 << (level + 1)); compare_box++) {
			/* Calculate base offset for the current box */
			int compare_box_offset = base_offset + compare_box * blocks_per_compare_box;

			if(level == max_level-1) {
				/* We've reached the block size, create task for the compare box part */
				stream_compare_halves(block_size / 2,
						      block_size,
						      &intra_streams[compare_box_offset],
						      &inter_streams[compare_box],
						      NULL,
						      NULL);
			} else {
				/* Further divide the compare box: one compare box part pêr block */
				for(int inner_compare_box = 0; inner_compare_box < blocks_per_compare_box / 2; inner_compare_box++) {
					/* Offset of the current compare box part */
					int offset = compare_box_offset + inner_compare_box;

					/* Output offset of the current compare box part */
					int offset_next_iter = offset + num_blocks;

					stream_compare_halves(block_size,
							      block_size,
							      &intra_streams[offset],
							      &intra_streams[offset_next_iter],
							      &intra_streams[offset+blocks_per_compare_box/2],
							      &intra_streams[offset_next_iter+blocks_per_compare_box/2]);
				}
			}
		}
	}
}

/* Generates tasks that read blocks of keys from the array of input keys and
 * copies them into the appropriate inter-stage streams.
 */
void stream_init(key_t* keys,
		 long num_keys,
		 long block_size,
		 void** inter_streams,
		 long num_inter_streams_total,
		 long i)
{
	key_t out[block_size];

	/* The number of streams used downards in the recursion is the total
	 * number of streams minus the number of streams used for output at
	 * the current step.
	 */
	long num_blocks = num_keys / block_size;
	long num_inter_streams_following = num_inter_streams_total - num_blocks;

	if(num_keys == block_size) {
		/* Block size reached, simply copy data from the input sequence
		 * to the stream. No further recursion is required.
		 */
		#pragma omp task output(inter_streams[1] << out[block_size])
		{
			memcpy(out, &keys[i*block_size], block_size*sizeof(key_t));
		}
	} else {
		/* Initilize streams for steps that indirectly provide input
		 * data to the current step. Divide remaining streams into top
		 * and bottom half with an offset of num_blocks (number of
		 * output streams of this step.
		 */

		stream_init(keys,
			    num_keys / 2,
			    block_size,
			    &inter_streams[num_blocks],
			    num_inter_streams_following / 2,
			    i);

		stream_init(keys,
			    num_keys / 2,
			    block_size,
			    &inter_streams[num_blocks + num_inter_streams_following / 2],
			    num_inter_streams_following / 2,
			    i + num_blocks / 2);
	}
}

/* Generate terminating tasks that read data from the final streams and copy it
 * to the output sequence.
 */
void stream_collect(key_t* keys, long num_keys, long block_size,
		    void** inter_streams, void** end_stream)
{
	long num_blocks = num_keys / block_size;
	int token;
	key_t out[block_size];

	/* We only need the output streams of the first reccursive step which
	 * are located at the front of the inter_streams table. Further, we
	 * need to write tokens into the end stream on which the cleanup
	 * task depends.
	 */
	for(long i = 0; i < num_blocks; i++) {
		#pragma omp task input(inter_streams[i] >> out[block_size])	  \
			output(end_stream[0] << token[1])		\
			firstprivate(i)
		{
			memcpy(&keys[i*block_size], out, block_size*sizeof(key_t));
		}
	}
}

int main(int argc, char** argv)
{
	/* log2 of the number of keys to sort */
	long num_keys_log = DEFAULT_NUM_KEYS_LOG;
	long num_keys = (1 << num_keys_log);

	/* log2 of the number of keys per block */
	long block_size_log = DEFAULT_BLOCK_SIZE_LOG;
	long block_size = 1 << block_size_log;

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
	key_t* keys = malloc(num_keys*sizeof(key_t));

	/* Number of parallel stages (stages with an number of output elements
	 * greater than the block size
	 */
	int num_par_stages = num_keys_log - block_size_log;

	/* Number of input / output blocks */
	int num_blocks = num_keys / block_size;

	/* Streams between parallel stages */
	int num_inter_stage_streams = (num_par_stages + 2) * num_blocks;
	key_t linter_streams[num_inter_stage_streams] __attribute__((stream));

	/* Streams within parallel stages */
	int num_intra_stage_streams = ((num_par_stages + 1) * num_par_stages)/2 * num_blocks;
	key_t lintra_streams[num_intra_stage_streams] __attribute__((stream));

	/* Stream for synchronization between the cleanup task and the
	 * last sorting tasks
	 */
	int lend_stream[1] __attribute__((stream));
	int token[num_blocks];

	/* Copy stream references from the stack to the heap */
	intra_streams = malloc(num_intra_stage_streams*sizeof(void*));
	memcpy(intra_streams, lintra_streams, num_intra_stage_streams*sizeof(void*));

	inter_streams = malloc(num_inter_stage_streams*sizeof(void*));
	memcpy(inter_streams, linter_streams, num_inter_stage_streams*sizeof(void*));

	end_stream = malloc(sizeof(void*));
	memcpy(end_stream, lend_stream, sizeof(void*));

	/* Initialize input array with random keys */
	init_sequence(keys, num_keys);

	printf("Start sorting %ld keys...\n", num_keys);

	gettimeofday (&start, NULL);
	PROFILER_NOTIFY_RECORD(&sync);

	/* Create initializing tasks */
	stream_init(keys, num_keys, block_size, inter_streams, num_inter_stage_streams, 0);

	/* Create sorting tasks */
	stream_sort(num_keys_log,
		    block_size_log,
		    inter_streams,
		    intra_streams,
		    num_inter_stage_streams,
		    num_intra_stage_streams);

	/* Create terminating tasks that collect final data */
	stream_collect(keys, num_keys, block_size, inter_streams, end_stream);

	/* Cleanup task */
	#pragma omp task input(end_stream[0] >> token[num_blocks])
	{
		printf("END taks\n");fflush(stdout);

		PROFILER_NOTIFY_PAUSE(&sync);
		gettimeofday (&end, NULL);

		printf ("%.5f\n", tdiff (&end, &start));fflush(stdout);
		PROFILER_NOTIFY_FINISH(&sync);

		if(check) {
			assert(check_ascending(keys, num_keys));
			printf("Check: OK\n");
		}

		free(keys);
		free(inter_streams);
		free(intra_streams);
		free(end_stream);
	}

	return 0;
}
