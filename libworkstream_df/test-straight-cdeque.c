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

#include "papi-defs.h"
#include "wstream_df.h"
#include "error.h"

#if !USE_STDATOMIC
#include "cdeque.h"
#else
#include "cdeque-c11.h"
#endif

static cdeque_t *deque;
static unsigned long num_iter;

int
main (int argc, char *argv[])
{
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

  deque = cdeque_alloc (6);
  assert (deque != NULL);

  for (i = 0; i < num_iter; ++i)
    cdeque_push_bottom (deque, NULL);
  for (i = 0; i < num_iter; ++i)
    (void) cdeque_take (deque);

  cdeque_free (deque);

  return 0;
}
