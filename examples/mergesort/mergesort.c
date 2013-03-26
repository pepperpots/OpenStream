/**
 * Simple sequential merge sort implementation.
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
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <malloc.h>
#include <getopt.h>
#include "mergesort.h"
#include "../common/common.h"
#include "../common/sync.h"

int main(int argc, char** argv)
{
  long num_keys_log = DEFAULT_NUM_KEYS_LOG;
  long num_keys = 1 << num_keys_log;
  key_t* keys_in;
  key_t* keys_out;
  struct timeval start;
  struct timeval end;
  struct profiler_sync sync;
  int option;
  int check = 0;

  while ((option = getopt(argc, argv, "s:hc")) != -1)
    {
      switch(option)
	{
	case 's':
	  num_keys_log = atol(optarg);
	  num_keys = 1 << num_keys_log;
	  break;
	case 'c':
	  check = 1;
	  break;
	case 'h':
	  printf("Usage: %s [option]...\n\n"
		 "Options:\n"
		 "  -s <power>                   Set the number of integer keys to 1 << <power>,\n"
		 "  -c                           Perform check at the end\n"
		 "                               default is %ld\n",
		 argv[0], num_keys_log);
	  exit(0);
	  break;
	case '?':
	  fprintf(stderr, "Run %s -h for usage.\n", argv[0]);
	  exit(1);
	  break;
	}
    }

  /* Allocate arrays for unsorted and sorted keys */
  keys_in = malloc(num_keys*sizeof(key_t));
  keys_out = malloc(num_keys*sizeof(key_t));

  assert(keys_in);
  assert(keys_out);

  /* Fill keys_in with random keys */
  init_sequence(keys_in, num_keys);

  printf("Start sorting %ld keys...\n", num_keys);

  gettimeofday (&start, NULL);
  PROFILER_NOTIFY_RECORD(&sync);

  /* Sort the array sequentially */
  sort_block(keys_in, keys_out, num_keys);

  PROFILER_NOTIFY_PAUSE(&sync);
  gettimeofday (&end, NULL);

  printf("End sorting...\n");

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
