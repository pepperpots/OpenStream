#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <getopt.h>
#include "../common/common.h"
#include "../common/sync.h"

#define _WITH_OUTPUT 0

#include <unistd.h>

int
fibo (int n)
{
  if (n < 2)
    return n;
  return fibo (n-1) + fibo (n-2);
}

void
bar_fibo (int n, int cutoff, int *result)
{
  int a, b;

  if (n <= cutoff)
    {
#pragma omp task firstprivate (n) firstprivate (result)
      *result = fibo (n);
    }
  else
    {
#pragma omp task firstprivate (n, cutoff, result) private (a, b)
      {
	bar_fibo (n - 1, cutoff, &a);
	bar_fibo (n - 2, cutoff, &b);
#pragma omp taskwait
	*result = a + b;
      }
    }
}


int
main (int argc, char **argv)
{
  int option;
  int n = 15;

  int cutoff = 10;
  int result;

  struct profiler_sync sync;

  PROFILER_NOTIFY_PREPARE(&sync);

  while ((option = getopt(argc, argv, "n:c:h")) != -1)
    {
      switch(option)
	{
	case 'n':
	  n = atoi(optarg);
	  break;
	case 'c':
	  cutoff = atoi(optarg);
	  break;
	case 'h':
	  printf("Usage: %s [option]...\n\n"
		 "Options:\n"
		 "  -n <number>                  Calculate fibonacci number <number>, default is %d\n"
		 "  -c <cutoff>                  Start generating tasks at n = <cutoff>, default is %d\n",
		 argv[0], n, cutoff);
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

  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));

  gettimeofday (start, NULL);
  PROFILER_NOTIFY_RECORD(&sync);
  bar_fibo (n, cutoff, &result);

#pragma omp taskwait

  PROFILER_NOTIFY_PAUSE(&sync);
  gettimeofday (end, NULL);

  printf ("%.5f\n", tdiff (end, start));

  if (_WITH_OUTPUT)
    printf ("[taskwait] Fibo (%d, %d) = %d\n", n, cutoff, result);

  PROFILER_NOTIFY_FINISH(&sync);

  return 0;
}
