#include <stdio.h>
#include <stdlib.h>
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
stream_fibo (int n, int cutoff, int sout __attribute__ ((stream)))
{
  int x;

  if (n <= cutoff)
    {
#pragma omp task output (sout << x)
      x = fibo (n);
    }
  else
    {
      int view[2];

      /* Use a single stream throughout the program, which is
	 recursively passed to callees to store their outputs.  */
      stream_fibo (n - 1, cutoff, sout);
      stream_fibo (n - 2, cutoff, sout);

#pragma omp task input (sout >> view[2]) output (sout << x)
      x = view[0] + view[1];
    }
}

struct profiler_sync sync;

int
main (int argc, char **argv)
{
  int option;
  int n = 15;

  int cutoff = 10;
  int result;

  int stream __attribute__ ((stream));

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
  stream_fibo (n, cutoff, stream);

#pragma omp task input (stream >> result) firstprivate (start, end)
  {
    PROFILER_NOTIFY_PAUSE(&sync);
    gettimeofday (end, NULL);

    printf ("%.5f\n", tdiff (end, start));

    if (_WITH_OUTPUT)
      printf ("[single stream] Fibo (%d, %d) = %d\n", n, cutoff, result);

    PROFILER_NOTIFY_FINISH(&sync);
  }

  return 0;
}
