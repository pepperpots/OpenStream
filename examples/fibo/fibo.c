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

int
main (int argc, char **argv)
{
  int option;
  int i, j, iter;
  int n = 15;

  int numiters = 10;
  int cutoff = 10;
  int result;

  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));

  while ((option = getopt(argc, argv, "n:c:r:")) != -1)
    {
      switch(option)
	{
	case 'n':
	  n = atoi(optarg);
	  break;
	case 'c':
	  cutoff = atoi(optarg);
	  break;
	case 'r':
	  numiters = atoi (optarg);
	  break;
	}
    }

  gettimeofday (start, NULL);
  result = fibo (n);
  gettimeofday (end, NULL);

  printf ("%.5f\n", tdiff (end, start));

  if (_WITH_OUTPUT)
    printf ("Fibo (%d) = %d\n", n, result);
}
