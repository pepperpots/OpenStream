#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <getopt.h>
#include "../common/common.h"

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
      /* In this version, we only use a stream for a single
	 synchronization.  The streams created here are passed to the
	 recursive calls which will only store their results inside,
	 then we read the results and the stream is done.  */
      int s1 __attribute__ ((stream));
      int s2 __attribute__ ((stream));
      int v1, v2;

      stream_fibo (n - 1, cutoff, s1);
      stream_fibo (n - 2, cutoff, s2);

#pragma omp task input (s1 >> v1, s2 >> v2) output (sout << x)
      x = v1 + v2;
    }
}


int
main (int argc, char **argv)
{
  int option;
  int n = 15;

  int cutoff = 10;
  int result;

  int stream __attribute__ ((stream));

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
  stream_fibo (n, cutoff, stream);

#pragma omp task input (stream >> result) firstprivate (start, end)
  {
    gettimeofday (end, NULL);

    printf ("%.5f\n", tdiff (end, start));

    if (_WITH_OUTPUT)
      printf ("[stream] Fibo (%d, %d) = %d\n", n, cutoff, result);
  }

  return 0;
}
