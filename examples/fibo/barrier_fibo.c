#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>

#include <getopt.h>

#define _WITH_OUTPUT 0

#include <sys/time.h>
#include <unistd.h>
double
tdiff (struct timeval *end, struct timeval *start)
{
  return (double)end->tv_sec - (double)start->tv_sec +
    (double)(end->tv_usec - start->tv_usec) / 1e6;
}


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
  if (n <= cutoff)
    {
#pragma omp task firstprivate (result, n)
      *result = fibo (n);
    }
  else
    {
#pragma omp task firstprivate (result, n, cutoff)
      {
	int a, b;
	int *pa = &a;
	int *pb = &b;
	//printf ("L0-%d:  %d %d\n", n, a, b);
	bar_fibo (n - 1, cutoff, pa);
	bar_fibo (n - 2, cutoff, pb);
	//printf ("L0-%d:  %d %d\n", n, a, b);
#pragma omp taskwait
	//printf ("L1-%d:  %d %d\n", n, a, b);
	*result = a + b;
      }
    }
}


int
main (int argc, char **argv)
{
  int option;
  int i, j, iter;
  int n = 15;

  int cutoff = 10;
  int result;

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
  bar_fibo (n, cutoff, &result);

#pragma omp taskwait

  gettimeofday (end, NULL);

  printf ("%.5f\n", tdiff (end, start));

  if (_WITH_OUTPUT)
    printf ("[stream] Fibo (%d, %d) = %d\n", n, cutoff, result);
}
