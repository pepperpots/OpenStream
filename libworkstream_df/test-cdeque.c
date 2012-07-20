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
#include "cdeque.h"

static pthread_t worker_thread, thief_thread;
static cdeque_t *worker_deque;

static unsigned int worker_seed, thief_seed;
static FILE *worker_log, *thief_log;

static unsigned long take_empty_count, steal_empty_count;
static unsigned long num_job, num_steal;
static unsigned long breadth, depth;

static volatile unsigned long num_start_spin = 1000000000UL;
static volatile bool start;

static int timespec_diff (struct timespec *,
			  const struct timespec *, const struct timespec *);

static void *worker_main (void *);
static void worker (unsigned long);
static void *thief_main (void *);

#define LOG(log, ...) ((void) ((log) != NULL && fprintf ((log), __VA_ARGS__)))

static int
timespec_diff (struct timespec *result,
	       const struct timespec *px, const struct timespec *py)
{
  struct timespec x, y;

  x = *px;
  y = *py;

  /* Perform the carry for the later subtraction by updating y. */
  if (x.tv_nsec < y.tv_nsec) {
    long ns = (y.tv_nsec - x.tv_nsec) / 1000000000L + 1;
    y.tv_nsec -= 1000000000L * ns;
    y.tv_sec += ns;
  }
  if (x.tv_nsec - y.tv_nsec > 1000000000L) {
    long ns = (x.tv_nsec - y.tv_nsec) / 1000000000L;
    y.tv_nsec += 1000000000L * ns;
    y.tv_sec -= ns;
  }

  /* Compute the time remaining to wait. tv_nsec is certainly
     positive. */
  result->tv_sec = x.tv_sec - y.tv_sec;
  result->tv_nsec = x.tv_nsec - y.tv_nsec;

  /* Return 1 if result is negative. */
  return x.tv_sec < y.tv_sec;
}

static void *
worker_main (void *data)
{
  struct timespec end_tv, start_tv, t;
  unsigned long i;

  for (i = 0; i < num_start_spin; ++i)
    continue;
  start = true;
  fprintf (stderr, "Worker started\n");

  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tv);

  worker (depth);

  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tv);
  assert (timespec_diff (&t, &end_tv, &start_tv) == 0);
  fprintf (stderr, "worker_time = %ld.%09ld\n", t.tv_sec, t.tv_nsec);

  return data;
}

static void
worker (unsigned long d)
{
  void *val;
  double dummy;
  unsigned long b;

  if (d == 0)
    return;
  for (b = 0; b < breadth; ++b)
    {
      dummy = (double) rand_r (&worker_seed) / RAND_MAX;
      cdeque_push_bottom (worker_deque, &dummy);
      worker (d - 1);
      val = cdeque_take (worker_deque);
      if (val == NULL)
	++take_empty_count;
      else
	assert (*(double *) val == dummy);
    }
}

static void *
thief_main (void *data)
{
  struct timespec end_tv, start_tv, t;
  void *val;
  double stealprob;
  unsigned long i;

  while (!start)
    continue;
  fprintf (stderr, "Thief started\n");

  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tv);

  stealprob = (double) num_steal / num_job;
  for (i = 0; i < num_steal; ++i)
    {
      /* Minimal probability is actually 1/RAND_MAX. */
      while ((double) rand_r (&thief_seed) / RAND_MAX > stealprob)
	continue;
      val = cdeque_steal (worker_deque);
      if (val == NULL)
	++steal_empty_count;
    }

  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tv);
  assert (timespec_diff (&t, &end_tv, &start_tv) == 0);
  fprintf (stderr, "thief_time = %ld.%09ld\n", t.tv_sec, t.tv_nsec);

  return data;
}

int
main (int argc, char *argv[])
{
  pthread_attr_t thrattr;
  cpu_set_t cpuset;
  size_t dqlogsize;
  unsigned long d;
  int opt;

  num_steal = 10000000;
  breadth = 6;
  depth = 10;
  dqlogsize = 6;
  while ((opt = getopt (argc, argv, "b:d:i:ln:r:")) != -1)
    {
      switch (opt)
	{
	case 'b':
	  breadth = strtoul (optarg, NULL, 0);
	  break;

	case 'd':
	  depth = strtoul (optarg, NULL, 0);
	  break;

	case 'i':
	  dqlogsize = strtoul (optarg, NULL, 0);
	  break;

	case 'l':
	  if (worker_log == NULL)
	    {
	      worker_log = fopen ("worker.log", "w");
	      thief_log = fopen ("thief.log", "w");
	      assert (worker_log != NULL);
	      assert (thief_log != NULL);
	    }
	  break;

	case 'n':
	  num_steal = strtoul (optarg, NULL, 0);
	  break;

	case 'r':
	  worker_seed = strtoul (optarg, NULL, 0);
	  thief_seed = worker_seed;
	  break;

	default:
	  fprintf (stderr, "Usage: %s [-d] [-i LOGSIZE] [-n NITER] [-r SEED]\n",
		   argv[0]);
	  return EXIT_FAILURE;
	}
    }

  num_job = 1;
  for (d = 0; d < depth; ++d)
    num_job *= breadth;

  worker_deque = cdeque_alloc (dqlogsize);
  assert (worker_deque != NULL);

  pthread_attr_init (&thrattr);
  CPU_ZERO (&cpuset);
  CPU_SET (0, &cpuset);
  assert (pthread_attr_setaffinity_np (&thrattr, sizeof cpuset, &cpuset) == 0);
  assert (pthread_create (&worker_thread, &thrattr, worker_main, NULL) == 0);

  pthread_attr_init (&thrattr);
  CPU_ZERO (&cpuset);
  CPU_SET (1, &cpuset);
  assert (pthread_attr_setaffinity_np (&thrattr, sizeof cpuset, &cpuset) == 0);
  assert (pthread_create (&thief_thread, &thrattr, thief_main, NULL) == 0);

  assert (pthread_join (worker_thread, NULL) == 0);
  assert (pthread_join (thief_thread, NULL) == 0);

  fprintf (stderr, "take_empty_count = %lu\n", take_empty_count);
  fprintf (stderr, "steal_empty_count = %lu\n", steal_empty_count);
  fprintf (stderr, "num_job = %lu\n", num_job);
  fprintf (stderr, "num_steal = %lu\n", num_steal);

  cdeque_free (worker_deque);

  if (worker_log != NULL)
    {
      fclose (worker_log);
      fclose (thief_log);
    }

  return 0;
}
