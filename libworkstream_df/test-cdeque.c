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
#include "time-util.h"

static pthread_t worker_thread, *thief_threads;
static struct timespec worker_time, *thief_times;
static cdeque_t *worker_deque;

static unsigned int worker_seed, *thief_seeds;

static unsigned long take_empty_count, steal_empty_count;
static unsigned long num_thread, num_job, num_steal, num_steal_per_thread;
static unsigned long breadth, depth;

static volatile unsigned long num_start_spin = 1000000000UL;
static volatile bool start, end;

static void *worker_main (void *);
static void worker (unsigned long);
static void *thief_main (void *);

static void *
worker_main (void *data)
{
  unsigned long i;

  for (i = 0; i < num_start_spin; ++i)
    continue;
  start = true;

  BEGIN_TIME (&worker_time);
  worker (depth);
  END_TIME (&worker_time);

  end = true;

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
  void *val;
  double stealprob;
  unsigned long cpu_id, i;

  while (!start)
    continue;

  cpu_id = (unsigned long) data;
  BEGIN_TIME (&thief_times[cpu_id]);

  stealprob = (double) num_steal_per_thread / num_job;
  for (i = 0; i < num_steal_per_thread; ++i)
    {
      /* Minimal probability is actually 1/RAND_MAX. */
      while ((double) rand_r (&thief_seeds[cpu_id]) / RAND_MAX > stealprob)
	continue;
      val = cdeque_steal (worker_deque);
      if (val == NULL)
	++steal_empty_count;
    }

  END_TIME (&thief_times[cpu_id]);

  while (!end)
    continue;

  return data;
}

int
main (int argc, char *argv[])
{
  pthread_attr_t thrattr;
  cpu_set_t cpuset;
  double stealratio;
  size_t dqlogsize;
  unsigned long d, t, nrowjob;
  int opt;

  num_thread = 2;
  num_steal = -1;
  stealratio = 0.01;
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
	  if (strchr (optarg, '.') == NULL)
	    {
	      num_steal = strtoul (optarg, NULL, 0);
	      stealratio = -1.0;
	    }
	  else
	    {
	      num_steal = -1;
	      stealratio = strtod (optarg, NULL);
	    }
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
		   "  -s NUM_STEAL_OR_RATIO\n",
		   argv[0]);
	  return EXIT_FAILURE;
	}
    }

  nrowjob = 1;
  for (d = 0; d < depth; ++d)
    {
      nrowjob *= breadth;
      num_job += nrowjob;
    }

  if (stealratio >= 0.0)
    num_steal = (unsigned long) (stealratio * num_job);
  num_steal_per_thread = num_steal / num_thread;
  num_steal = num_steal_per_thread * num_thread;

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
#if !NO_TEST_SETAFFINITY
      assert (pthread_attr_setaffinity_np (&thrattr,
					   sizeof cpuset, &cpuset) == 0);
#endif
      assert (pthread_create (&thief_threads[t],
			      &thrattr, thief_main, (void *) t) == 0);
    }

  pthread_attr_init (&thrattr);
  CPU_ZERO (&cpuset);
  CPU_SET (0, &cpuset);
#if !NO_TEST_SETAFFINITY
  assert (pthread_attr_setaffinity_np (&thrattr, sizeof cpuset, &cpuset) == 0);
#endif
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
