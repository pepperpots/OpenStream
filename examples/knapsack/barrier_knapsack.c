/*
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Matteo Frigo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include "../common/common.h"

#define _WITH_OUTPUT 1

/* every item in the knapsack has a weight and a value */
#define MAX_ITEMS 256

#include <unistd.h>

struct item
{
  int value;
  int weight;
};

int best_so_far = INT_MIN;

static inline int
compare (struct item *a, struct item *b)
{
  double c = ((double) a->value / a->weight) -
    ((double) b->value / b->weight);

  if (c > 0)
    return -1;
  if (c < 0)
    return 1;
  return 0;
}

int
read_input (const char *filename, struct item *items, int *capacity, int *n)
{
  int i;
  FILE *f;

  if (filename == NULL)
    filename = "\0";
  f = fopen(filename, "r");
  if (f == NULL) {
    fprintf(stderr, "open_input(\"%s\") failed\n", filename);
    return -1;
  }
  /* format of the input: #items capacity value1 weight1 ... */
  fscanf(f, "%d", n);
  fscanf(f, "%d", capacity);

  for (i = 0; i < *n; ++i)
    fscanf(f, "%d %d", &items[i].value, &items[i].weight);

  fclose(f);

  /* sort the items on decreasing order of value/weight */
  /* cilk2c is fascist in dealing with pointers, whence the ugly cast */
  qsort(items, *n, sizeof(struct item),
	(int (*)(const void *, const void *)) compare);

  return 0;
}

/*
 * return the optimal solution for n items (first is e) and
 * capacity c. Value so far is v.
 */
void
stream_knapsack (struct item *e, int c, int n, int v, int *sol)
{
  int with, without, best;
  double ub;

  /* base case: full knapsack or no items */
  if (c < 0)
    {
      *sol = INT_MIN;
      return;
    }
  if (n == 0 || c == 0)
    {
      *sol = v;
      return;
    }

#pragma omp task
  {

    ub = (double) v + c * e->value / e->weight;

    if (ub >= best_so_far)
      {
	/*
	 * compute the best solution without the current item in the knapsack
	 */
	stream_knapsack (e + 1, c, n - 1, v, &without);

	/* compute the best solution with the current item in the knapsack */
	stream_knapsack (e + 1, c - e->weight, n - 1, v + e->value, &with);

#pragma omp taskwait

	best = with > without ? with : without;

	/*
	 * notice the race condition here. The program is still
	 * correct, in the sense that the best solution so far
	 * is at least best_so_far. Moreover best_so_far gets updated
	 * when returning, so eventually it should get the right
	 * value. The program is highly non-deterministic.
	 */
	if (best > best_so_far)
	  best_so_far = best;

	*sol = best;
      }
    else
      {
	*sol = INT_MIN;
      }
  }
}


int
main (int argc, char *argv[])
{
  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));
  struct item items[MAX_ITEMS];	/* array of items */
  int n, capacity, sol, benchmark, help, option;
  char filename[100];


  while ((option = getopt(argc, argv, "n:h")) != -1)
    {
      switch(option)
	{
	case 'n':
	  benchmark = atoi(optarg);
	  break;
	case 'h':
	  printf("Usage: %s [option]...\n\n"
		 "Options:\n"
		 "  -n <benchmark>               Benchmark number; selects input file knapsack-example<benchmark>.input\n"
		 "                               Valid values are 1, 2, 3 or 4\n",
		 argv[0]);
	  exit(0);
	  break;
	case '?':
	  fprintf(stderr, "Run %s -h for usage.\n", argv[0]);
	  exit(1);
	  break;
	}
    }

  if(optind != argc) {
	  fprintf(stderr, "Too many arguments. Run %s -h for usage.\n", argv[0]);
	  exit(1);
  }

  if (benchmark < 1 || benchmark > 4)
    /* standard benchmark options */
    benchmark = 2;

  if (benchmark) {
    switch (benchmark) {
    case 1:		/* short benchmark options -- a little work */
      strcpy(filename, "knapsack-example1.input");
      break;
    case 2:		/* standard benchmark options */
      strcpy(filename, "knapsack-example2.input");
      break;
    case 3:		/* long benchmark options -- a lot of work */
      strcpy(filename, "knapsack-example3.input");
      break;
    case 4:		/* long benchmark options -- a lot of work */
      strcpy(filename, "knapsack-example4.input");
      break;
    }
  }
  if (read_input(filename, items, &capacity, &n))
    return 1;

  gettimeofday (start, NULL);

  stream_knapsack (items, capacity, n, 0, &sol);
#pragma omp taskwait

  gettimeofday (end, NULL);
  printf ("%.5f\n", tdiff (end, start));

  if (_WITH_OUTPUT)
    printf("Best value is %d\n\n", sol);

  return 0;
}
