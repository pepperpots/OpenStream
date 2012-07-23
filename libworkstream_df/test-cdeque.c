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

static pthread_t worker_thread, *thief_threads;
static struct timespec worker_time, *thief_times;
static cdeque_t *worker_deque;

static unsigned int worker_seed, *thief_seeds;

static unsigned long take_empty_count, steal_empty_count;
static unsigned long num_thread, num_job, num_steal;
static unsigned long breadth, depth;

static volatile unsigned long num_start_spin = 1000000000UL;
static volatile bool start;

static int timespec_diff (struct timespec *,
			  const struct timespec *, const struct timespec *);

static void *worker_main (void *);
static void worker (unsigned long);
static void *thief_main (void *);

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
  struct timespec end_tv, start_tv;
  unsigned long i;

  for (i = 0; i < num_start_spin; ++i)
    continue;
  start = true;

  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tv);

  worker (depth);

  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tv);
  assert (timespec_diff (&worker_time, &end_tv, &start_tv) == 0);

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
  struct timespec end_tv, start_tv;
  void *val;
  double stealprob;
  unsigned long cpu_id, i;

  while (!start)
    continue;

  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tv);

  cpu_id = (unsigned long) data;
  stealprob = (double) num_steal / num_job;
  for (i = 0; i < num_steal; ++i)
    {
      /* Minimal probability is actually 1/RAND_MAX. */
      while ((double) rand_r (&thief_seeds[cpu_id]) / RAND_MAX > stealprob)
	continue;
      val = cdeque_steal (worker_deque);
      if (val == NULL)
	++steal_empty_count;
    }

  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tv);
  assert (timespec_diff (&thief_times[cpu_id], &end_tv, &start_tv) == 0);

  return data;
}

int
main (int argc, char *argv[])
{
  pthread_attr_t thrattr;
  cpu_set_t cpuset;
  size_t dqlogsize;
  unsigned long d, t;
  int opt;

  num_thread = 2;
  num_steal = 10000000;
  breadth = 6;
  depth = 10;
  dqlogsize = 6;
  while ((opt = getopt (argc, argv, "b:d:i:n:r:s:")) != -1)
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

	case 'n':
	  num_thread = strtoul (optarg, NULL, 0);
	  if (num_thread < 2)
	    {
	      fprintf (stderr, "-n NUM_THREAD must be greater or equal to 2");
	    }
	  break;

	case 'r':
	  srand (strtoul (optarg, NULL, 0));
	  break;

	case 's':
	  num_steal = strtoul (optarg, NULL, 0);
	  break;

	default:
	  fprintf (stderr,
		   "Usage: %s [OPTIONS]\n"
		   "Options:\n"
		   "  -b BREADTH\n"
		   "  -d DEPTH\n"
		   "  -i INI_DEQUE_LOG_SIZE\n"
		   "  -n NUM_THREAD\n"
		   "  -r RAND_SEED\n"
		   "  -s NUM_STEAL_PER_THREAD\n",
		   argv[0]);
	  return EXIT_FAILURE;
	}
    }

  num_job = 1;
  for (d = 0; d < depth; ++d)
    num_job *= breadth;

  worker_deque = cdeque_alloc (dqlogsize);
  assert (worker_deque != NULL);

  thief_times = malloc (num_thread * sizeof *thief_times);
  assert (thief_times != NULL);
  thief_threads = malloc (num_thread * sizeof *thief_threads);
  assert (thief_threads != NULL);
  thief_seeds = malloc (num_thread * sizeof *thief_seeds);
  assert (thief_seeds != NULL);

  worker_seed = rand ();
  for (t = 1; t < num_thread; ++t)
    {
      thief_seeds[t] = rand ();

      pthread_attr_init (&thrattr);
      CPU_ZERO (&cpuset);
      CPU_SET (t, &cpuset);
      assert (pthread_attr_setaffinity_np (&thrattr,
					   sizeof cpuset, &cpuset) == 0);
      assert (pthread_create (&thief_threads[t],
			      &thrattr, thief_main, (void *) t) == 0);
    }

  pthread_attr_init (&thrattr);
  CPU_ZERO (&cpuset);
  CPU_SET (0, &cpuset);
  assert (pthread_attr_setaffinity_np (&thrattr, sizeof cpuset, &cpuset) == 0);
  assert (pthread_create (&worker_thread, &thrattr, worker_main, NULL) == 0);

  assert (pthread_join (worker_thread, NULL) == 0);
  for (t = 1; t < num_thread; ++t)
    assert (pthread_join (thief_threads[t], NULL) == 0);

  fprintf (stderr, "worker_time = %ld.%09ld\n",
	   worker_time.tv_sec, worker_time.tv_nsec);
  for (t = 1; t < num_thread; ++t)
    {
      fprintf (stderr, "thief_time #%lu = %ld.%09ld\n",
	       t, thief_times[t].tv_sec, thief_times[t].tv_nsec);
    }

  fprintf (stderr, "take_empty_count = %lu\n", take_empty_count);
  fprintf (stderr, "steal_empty_count = %lu\n", steal_empty_count);
  fprintf (stderr, "num_job = %lu\n", num_job);
  fprintf (stderr, "num_steal = %lu\n", num_steal);

  free (thief_seeds);
  free (thief_threads);
  free (thief_times);

  cdeque_free (worker_deque);

  return 0;
}
