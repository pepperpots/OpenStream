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

int
main (int argc, char **argv)
{
  int option;
  int n = 15;

  int result;

  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));

  while ((option = getopt(argc, argv, "n:h")) != -1)
    {
      switch(option)
	{
	case 'n':
	  n = atoi(optarg);
	  break;
	case 'h':
	  printf("Usage: %s [option]...\n\n"
		 "Options:\n"
		 "  -n <number>                  Calculate fibonacci number <number>, default is %d\n",
		 argv[0], n);
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

  gettimeofday (start, NULL);
  result = fibo (n);
  gettimeofday (end, NULL);

  printf ("%.5f\n", tdiff (end, start));

  if (_WITH_OUTPUT)
    printf ("Fibo (%d) = %d\n", n, result);

  return 0;
}
