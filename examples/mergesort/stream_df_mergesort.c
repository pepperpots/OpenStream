/**
 * Simple merge sort implementation, data flow version.
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
#include <assert.h>
#include <malloc.h>
#include <getopt.h>
#include "mergesort.h"
#include "../common/common.h"
#include "../common/sync.h"

/* Perfoms a dataflow merge sort on keys_in and writes the result to keys_out.
 * The number of keys has to be a power of two, the logarithm to the base 2
 * has to be specified using num_keys_log.
 *
 * In order to limit parallel overhead, blocks smaller than a certain size
 * are sorted sequentially. The logarithm to the base 2 of the block size
 * hasd to be specified using block_size_log.
 */
void sort_block_df(key_t* keys_in, key_t* keys_out, long num_keys_log, long block_size_log)
{
  /* For each level l there are 2^l streams and the total number of levels
   * is L = num_keys_log - block_size_log. Altogether, there are
   * 2^L + 2^L - 1 = 2^(L+1) - 1 streams.
   */
  long num_streams = (1 << (num_keys_log - block_size_log + 1)) - 1;
  key_t streams[num_streams] __attribute__((stream));

  /* One stream is used in order to pass the final token to
   * the enveloping task.
   */
  int end_token __attribute__((stream));

  /* Starts with block size specified by block_size_log */
  for(long ibsl = block_size_log; ibsl < num_keys_log; ibsl++) {
    /* ib: current block number at the current level */
    for(long ib = 0; ib < (1 << (num_keys_log - ibsl)); ib += 2) {
      /* Two input streams from the lower level: right and left */
      long stream_no_left = (1 << (num_keys_log - ibsl))-1 + ib;
      long stream_no_right = stream_no_left + 1;

      /* One output stream at the current level */
      long stream_no_out = (1 << (num_keys_log - ibsl - 1))-1 + ib/2;

      /* Block size */
      long ibs = 1 << ibsl;
      key_t left[ibs];
      key_t right[ibs];
      key_t out[2*ibs];

      if(ibsl == block_size_log) {
	/* Initial task that sorts a block and writes the sorted sequence
	 * to the output streams */
	#pragma omp task output(streams[stream_no_left] << left[ibs],	\
			        streams[stream_no_right] << right[ibs])
	{
	  sort_block(&keys_in[ib*ibs], left, ibs);
	  sort_block(&keys_in[(ib+1)*ibs], right, ibs);
	}
      }

      if(ibsl == num_keys_log-1) {
	/* Middle task that merges data from the two sorted input streams
	* and writes it to the output stream. */
	#pragma omp task input(streams[stream_no_left] >> left[ibs],	\
		               streams[stream_no_right] >> right[ibs])	\
			output(end_token)
	{
	  merge(left, right, keys_out, ibs);
	}
      } else {
	/* Final task that reads sorted data from the last two streams,
	 * merges it and writes the result to the output array. A token is
	 * put into the stream indicating that the sort has terminated.
	 */
	#pragma omp task input(streams[stream_no_left] >> left[ibs],	\
			       streams[stream_no_right] >> right[ibs])	\
		        output(streams[stream_no_out] << out[2*ibs])
	{
	  merge(left, right, out, ibs);
	}
      }
    }
  }

  /* If the block size is equal to the number of keys to be sorted,
   * then sort entirely sequentially
   */
  if(block_size_log == num_keys_log) {
    sort_block(keys_in, keys_out, 1 << block_size_log);
  } else {
    /* Wait for parallel sort to end */
    #pragma omp task input(end_token)
    {
      ;;
    }

    #pragma omp taskwait
  }
}

int main(int argc, char** argv)
{
  long num_keys_log = DEFAULT_NUM_KEYS_LOG;
  long num_keys = 1 << num_keys_log;
  long block_size_log = DEFAULT_BLOCK_SIZE_LOG;
  key_t* keys_in;
  key_t* keys_out;
  struct timeval start;
  struct timeval end;
  struct profiler_sync sync;
  int option;
  int check = 0;

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

  /* Allocate arrays for unsorted and sorted keys */
  keys_in = malloc(num_keys*sizeof(key_t));
  keys_out = malloc(num_keys*sizeof(key_t));
  assert(keys_in);
  assert(keys_out);

  /* Fill keys_in with random keys */
  init_sequence(keys_in, num_keys);

  gettimeofday (&start, NULL);
  PROFILER_NOTIFY_RECORD(&sync);
  printf("Start sorting %ld keys...\n", num_keys);

  /* Sort the array in parallel */
  sort_block_df(keys_in, keys_out, num_keys_log, block_size_log);

  printf("End sorting...\n");
  PROFILER_NOTIFY_PAUSE(&sync);
  gettimeofday (&end, NULL);

  printf("%.5f\n", tdiff(&end, &start));

  if(check) {
    assert(check_ascending(keys_out, num_keys));
    printf("Check: OK\n");
  }

  free(keys_in);
  free(keys_out);
  PROFILER_NOTIFY_FINISH(&sync);

  return 0;
}
