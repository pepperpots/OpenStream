#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wstream_df.h"
#include "error.h"
#include "cdeque.h"

static pthread_t worker_thread, thief_thread;
static cdeque_t *worker_deque;

static unsigned int worker_seed, thief_seed;
static unsigned int worker_nempty, thief_nempty;
static FILE *worker_log, *thief_log;

static void *worker_main (void *);
static void *thief_main (void *);

static void *
worker_main (void *data)
{
  intptr_t i, n, val;

  n = *(intptr_t *) data;
  val = 0;
  for (i = 1; i < n;)
    {
      if (rand_r (&worker_seed) % 2 == 0)
	{
	  val = (intptr_t) cdeque_take (worker_deque);
	  if (val == 0)
	    ++worker_nempty;
	  else
	    fprintf (worker_log, "%jd\n", (intmax_t) val);
	}
      else
	cdeque_push_bottom (worker_deque, (void *) i++);
    }
  for (;;)
    {
      val = (intptr_t) cdeque_take (worker_deque);
      if (val == 0)
	break;
      else
	fprintf (worker_log, "%jd\n", (intmax_t) val);
    }
  return NULL;
}

static void *
thief_main (void *data)
{
  intptr_t val;

  for (;;)
    {
      val = (intptr_t) cdeque_steal (worker_deque);
      if (val == 0)
	++thief_nempty;
      else
	fprintf (thief_log, "%jd\n", (intmax_t) val);
      sleep (0);
    }
  return data;
}

#define perror_and_exit_failure(err_message) \
  { perror(err_message); exit(EXIT_FAILURE); }

int
main (int argc, char *argv[])
{
  intptr_t n;

  n = argc >= 2 ? atoi(argv[1]) : 100000;
  worker_seed = argc >= 3 ? (unsigned int) atoi(argv[2]) : 0;
  thief_seed = argc >= 4 ? (unsigned int) atoi(argv[3]) : 0;

  worker_log = fopen ("worker.log", "w");
  thief_log = fopen ("thief.log", "w");
  assert (worker_log != NULL);
  assert (thief_log != NULL);

  worker_deque = cdeque_alloc (6);
  assert (worker_deque != NULL);


  if (pthread_create (&worker_thread, NULL, worker_main, &n))
    perror_and_exit_failure("pthread_create:");
  if (pthread_create (&thief_thread, NULL, thief_main, NULL))
    perror_and_exit_failure("pthread_create:");
  if (pthread_join (worker_thread, NULL))
    perror_and_exit_failure("pthread_join:");
  if (pthread_cancel (thief_thread))
    perror_and_exit_failure("pthread_cancel:");
  if (pthread_join (thief_thread, NULL))
    perror_and_exit_failure("pthread_join:");

  fprintf (stderr, "(worker) nempty = %u\n", worker_nempty);
  fprintf (stderr, "(thief) nempty = %u\n", thief_nempty);

  cdeque_free (worker_deque);

  fclose (worker_log);
  fclose (thief_log);

  return 0;
}
