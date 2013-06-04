/**
 * Simple sequential implementation of Batcher's bitonic merge.
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
#include <getopt.h>
#include "../common/common.h"
#include "../common/sync.h"

/* Compare upper and lower half in a triangular fashion:
 * top_in[i] is compared to bottom_in[num_keys-i-1].
 */
void triangle_merge(key_t* a, key_t* b, long num_keys)
{
	for(long i = 0; i < num_keys; i++)
		cmpxchg(&a[i], &b[num_keys-i-1]);
}

/* Recursively execute the algorithm's sorting boxes. A sorting box for a
 * sequence of keys contains a triangle merge followed by top-bottom
 * comparisons with a decreasing number of input lines.
 *
 * At each step of the recursion, the number of keys is divided by two. If it
 * is equal to the block size, then a merge sort instead of a bitonic merge
 * is performed.
 */
void sort(key_t* a, key_t* b, long num_keys, long block_size)
{
	/* Trivial case: 0 keys are always sorted */
	if(num_keys == 0)
		return;

	/* If we reach the block size, perform a merge sort */
	if(num_keys <= block_size) {
		key_t tmp[block_size];
		mergesort_block(a, tmp, num_keys);
		memcpy(a, tmp, num_keys*sizeof(key_t));

		mergesort_block(b, tmp, num_keys);
		memcpy(b, tmp, num_keys*sizeof(key_t));
	} else {
		sort(a, &a[num_keys/2], num_keys/2, block_size);
		sort(b, &b[num_keys/2], num_keys/2, block_size);
	}

	triangle_merge(a, b, num_keys);

	/* Compare boxes */
	for(long level = num_keys; level > 1; level /= 2) {
		for(long i = 0; i < num_keys; i += level) {
			compare_halves(&a[i], &a[i+level/2], level/2);
			compare_halves(&b[i], &b[i+level/2], level/2);
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

	key_t* keys;

	struct timeval start;
	struct timeval end;
	struct profiler_sync sync;

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
	keys = malloc(num_keys*sizeof(key_t));

	/* Initialize input array with random keys */
	init_sequence(keys, num_keys);

	printf("Start sorting %ld keys...\n", num_keys);

	gettimeofday (&start, NULL);
	PROFILER_NOTIFY_RECORD(&sync);

	sort(keys, &keys[num_keys/2], num_keys/2, block_size);

	PROFILER_NOTIFY_PAUSE(&sync);
	gettimeofday (&end, NULL);

	printf ("%.5f\n", tdiff (&end, &start));fflush(stdout);
	PROFILER_NOTIFY_FINISH(&sync);

	if(check) {
		assert(check_ascending(keys, num_keys));
		printf("Check: OK\n");
	}

	free(keys);
	return 0;
}
