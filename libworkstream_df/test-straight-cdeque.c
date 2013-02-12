#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "wstream_df.h"
#include "error.h"
#include "cdeque.h"
#include "time-util.h"

static cdeque_t *deque;
static unsigned long num_iter;

int
main (int argc, char *argv[])
{
  struct timespec tv;
  unsigned long i;
  int opt;

  num_iter = 60000000;
  while ((opt = getopt (argc, argv, "n:")) != -1)
    {
      switch (opt)
	{
	case 'n':
	  num_iter = strtoul (optarg, NULL, 0);
	  break;

	default:
	  fprintf (stderr, "Usage: %s [-n NITER]\n", argv[0]);
	  return EXIT_FAILURE;
	}
    }

  deque = cdeque_alloc (26);
  assert (deque != NULL);

  BEGIN_TIME (&tv);
  for (i = 0; i < num_iter; ++i)
    cdeque_push_bottom (deque, NULL);
  for (i = 0; i < num_iter; ++i)
    (void) cdeque_take (deque);
  END_TIME (&tv);
  fprintf (stderr, "time = %ld.%09ld\n", tv.tv_sec, tv.tv_nsec);

  cdeque_free (deque);

  return 0;
}
