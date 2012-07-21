#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define _WSTREAM_DF_DEBUG 1

//#define _PAPI_PROFILE

#ifdef _PAPI_PROFILE
#include <papi.h>
#endif

#define _PAPI_COUNTER_SETS 3

# define _PAPI_P0B
# define _PAPI_P0E
# define _PAPI_P1B
# define _PAPI_P1E
# define _PAPI_P2B
# define _PAPI_P2E
# define _PAPI_P3B
# define _PAPI_P3E

# define _PAPI_INIT_CTRS(I)
# define _PAPI_DUMP_CTRS(I)

#ifdef _PAPI_PROFILE
# define _PAPI_P0B start_papi_counters (0)
# define _PAPI_P0E stop_papi_counters (0)
# define _PAPI_P1B start_papi_counters (1)
# define _PAPI_P1E stop_papi_counters (1)
# define _PAPI_P2B start_papi_counters (2)
# define _PAPI_P2E stop_papi_counters (2)
//# define _PAPI_P3B start_papi_counters (3)
//# define _PAPI_P3E stop_papi_counters (3)

# define _PAPI_INIT_CTRS(I)			\
  {						\
    int _ii;					\
    for (_ii = 0; _ii < I; ++ _ii)		\
      init_papi_counters (_ii);			\
  }

# define _PAPI_DUMP_CTRS(I)			\
  {						\
    int _ii;					\
    for (_ii = 0; _ii < I; ++ _ii)		\
      dump_papi_counters (_ii);			\
  }
#endif

#include "wstream_df.h"
#include "error.h"
#include "cdeque.h"

//#define _PRINT_STATS

#ifdef _PAPI_PROFILE
#define _papi_num_events 4
int _papi_tracked_events[_papi_num_events] =
  {PAPI_TOT_CYC, PAPI_L1_DCM, PAPI_L2_DCM, PAPI_L3_TCM};
//  {PAPI_TOT_CYC, PAPI_L1_DCM, PAPI_L2_DCM, PAPI_TLB_DM, PAPI_PRF_DM, PAPI_MEM_SCY};
#endif


/***************************************************************************/
/* Implement linked list operations:
 *
 * - structures must contain a pointer to the next element as the
 *   first struct field.
 * - initialization can rely on calloc'ed stream data structure
 * - currently used for implementing streams in cases of
 *   single-threaded control program.
 */
/***************************************************************************/

typedef struct wstream_df_list_element
{
  struct wstream_df_list_element * next;

} wstream_df_list_element_t, *wstream_df_list_element_p;

typedef struct wstream_df_list
{
  wstream_df_list_element_p first;
  wstream_df_list_element_p last;
  wstream_df_list_element_p active_peek_chain;

} wstream_df_list_t, *wstream_df_list_p;

static inline void
wstream_df_list_init (wstream_df_list_p list)
{
  list->first = NULL;
  list->last = NULL;
}

static inline void
wstream_df_list_push (wstream_df_list_p list, void *e)
{
  wstream_df_list_element_p elem = (wstream_df_list_element_p) e;
  wstream_df_list_element_p prev_last;

  elem->next = NULL;

  if (list->first == NULL)
    {
      list->first = elem;
      list->last = elem;
      return;
    }

  prev_last = list->last;
  list->last = elem;
  prev_last->next = elem;
}

static inline wstream_df_list_element_p
wstream_df_list_pop (wstream_df_list_p list)
{
  wstream_df_list_element_p first = list->first;

  /* If the list is empty.  */
  if (first == NULL)
    return NULL;

  if (first->next == NULL)
    list->last = NULL;

  list->first = first->next;
  return first;
}

static inline wstream_df_list_element_p
wstream_df_list_head (wstream_df_list_p list)
{
  return list->first;
}

/***************************************************************************/
/* Data structures for T*.  */
/***************************************************************************/

struct barrier;

typedef struct wstream_df_frame
{
  size_t continuation_label_id;
  void (*work_fn) (void);
  int synchronization_counter;

  struct barrier *own_barrier;
  /* Variable size struct */
  //char buf [];
} wstream_df_frame_t, *wstream_df_frame_p;


typedef struct wstream_df_view
{
  /* MUST always be 1st field.  */
  void *next;
  /* MUST always be 2nd field.  */
  void *data;
  /* MUST always be 3rd field.  */
  void *sibling;
  size_t burst;
  size_t horizon;
  void *owner;
  /* re-use the dummy view's reached position to store the size of an
     array of views.  */
  size_t reached_position;
} wstream_df_view_t, *wstream_df_view_p;

/* The stream data structure.  It only relies on two linked lists of
   views in the case of a sequential control program (or at least
   sequential with respect to production and consumption of data in
   any given stream), or two single-producer single-consumer queues if
   the production and consumption of data is scheduled by independent
   control program threads.  */
typedef struct wstream_df_stream
{
  wstream_df_list_t producer_queue __attribute__((aligned (64)));
  wstream_df_list_t consumer_queue __attribute__((aligned (64)));

  size_t elem_size;
  int refcount;
} wstream_df_stream_t, *wstream_df_stream_p;

/* T* worker threads have each their own private work queue, which
   contains the frames that are ready to be executed.  For now the
   control program will be distributing work round-robin, later we
   will add work-sharing to make this more load-balanced.  A
   single-producer single-consumer concurrent dequeue could be used in
   this preliminary stage, which would require no atomic operations,
   but we'll directly use a multi-producer single-consumer queue
   instead.  */
typedef struct __attribute__ ((aligned (64))) wstream_df_thread
{
  pthread_t posix_thread_id;

  cdeque_t work_deque __attribute__((aligned (64)));
  bool volatile terminate_p __attribute__((aligned (64)));

  int worker_id;
  wstream_df_frame_p own_next_cached_thread;

#ifdef _PAPI_PROFILE
  int _papi_eset[16];
  long long counters[16][_papi_num_events];
#endif
} wstream_df_thread_t, *wstream_df_thread_p;


typedef union barrier_counter
{
  unsigned long long __attribute__((aligned (64))) counter;
  char fake[8]; // Just make sure that we can align on 64 bytes in arrays.

} barrier_counter;

typedef struct barrier
{
  bool cp_barrier;
  bool barrier_ready;
  int continuation_is_scheduled;
  wstream_df_frame_p continuation_frame;
  barrier_counter created;
  barrier_counter executed[];

} barrier_t, *barrier_p;

/***************************************************************************/
/***************************************************************************/
/* The current frame pointer is stored here in TLS */
static __thread wstream_df_frame_p current_fp;
static __thread barrier_p current_barrier;
static int num_workers;
static wstream_df_thread_p wstream_df_worker_threads;
static __thread wstream_df_thread_p current_thread = NULL;

/*************************************************************************/
/*******             BARRIER/SYNC Handling                         *******/
/*************************************************************************/

size_t
wstream_df_get_continuation_id ()
{
  return current_fp->continuation_label_id;
}

void
wstream_df_create_barrier ()
{
  unsigned int barrier_size = (sizeof (barrier_t)
			       + sizeof (barrier_counter) * (num_workers));
  barrier_p barrier = (barrier_p) calloc (1, barrier_size);
  int i;

  barrier->cp_barrier = false;
  barrier->barrier_ready = false;
  barrier->continuation_frame = NULL;
  barrier->continuation_is_scheduled = 0;

  barrier->created.counter = 0;
  for (i = 0; i < num_workers; ++i)
    barrier->executed[i].counter = 0;

  current_barrier = barrier;
}

/* Save self frame with the proper jump label identifier for
   continuation once the barrier passes.  */
void
wstream_df_schedule_continuation (size_t cont_id)
{
  /* ERROR if no barrier is associated.  */
  if (current_barrier == NULL)
    wstream_df_fatal ("Attempting to release a barrier without having created one.");

  current_fp->continuation_label_id = cont_id;

  /* Make barrier passable or free barrier and schedule the
     continuation if the barrier synchronizes no tasks.  */
  if (current_barrier->created.counter == 0)
    {
      free (current_barrier);
      if (current_thread->own_next_cached_thread == NULL)
	current_thread->own_next_cached_thread = current_fp;
      else
	cdeque_push_bottom (&current_thread->work_deque,
			    (wstream_df_type) current_fp);
    }
  else
    {
      current_barrier->continuation_frame = current_fp;
      current_barrier->barrier_ready = true;
    }
  /* Release barrier from its owner.  */
  current_barrier = NULL;
}

static inline bool
try_pass_barrier (barrier_p bar)
{
  unsigned long long executed = 0;
  int i;

  if (bar->barrier_ready == false)
    return false;

  for (i = 0; i < num_workers; ++i)
    executed += bar->executed[i].counter;
  if (bar->created.counter == executed)
    {
      /* If this is not a CP barrier, and we make sure a single thread
	 is allowed to schedule the continuation on its queue, using
	 an atomic CAS, then we schedule this continuation on the
	 thread's queue.  Otherwise simply return true, which is
	 meaningless for all but the CP.  */
      if (bar->cp_barrier == false)
	if (__sync_bool_compare_and_swap (&bar->continuation_is_scheduled, 0, 1))
	  {
	    if (current_thread->own_next_cached_thread == NULL)
	      current_thread->own_next_cached_thread = bar->continuation_frame;
	    else
	      cdeque_push_bottom (&current_thread->work_deque,
				  (wstream_df_type) bar->continuation_frame);
	  }
      return true;
    }
  return false;
}

static inline void
try_execute_one_task (cdeque_p sched_deque, unsigned int *rands)
{
  const unsigned int wid = current_thread->worker_id;
  wstream_df_frame_p fp = current_thread->own_next_cached_thread;
  __compiler_fence;

  if (fp == NULL)
    fp = (wstream_df_frame_p)  (cdeque_take (sched_deque));
  else
    current_thread->own_next_cached_thread = NULL;

  if (fp == NULL)
    {
      // Cheap alternative to nrand48
      unsigned int steal_from;
      *rands = *rands * 1103515245 + 12345;
      steal_from = *rands % num_workers;
      if (__builtin_expect (steal_from != wid, 1))
	fp = cdeque_steal (&wstream_df_worker_threads[steal_from].work_deque);
    }

  if (fp != NULL)
    {
      current_fp = fp;
      current_barrier = NULL;

      _PAPI_P3B;
      fp->work_fn ();
      _PAPI_P3E;

      __compiler_fence;
    }
}


void
wstream_df_taskwait (size_t s)
{
  unsigned int rands = 77773;
  barrier_p bar = current_barrier;

  /* Make barrier active, but tagged so nobody tries to pass
     it... worst case, we also flag it as already scheduled.  */
  bar->cp_barrier = true;
  bar->continuation_is_scheduled = 1;
  bar->continuation_frame = NULL;
  bar->barrier_ready = true;

  while (!try_pass_barrier (bar))
    try_execute_one_task (&current_thread->work_deque, &rands);

  /* Executing tasks may clobber current_barrier.  Use saved
     version.  */
  free (bar);
  current_barrier = NULL;

  /* Create the next barrier (which may not be used/useful if no
     further taskwaits occur -- can be switched off with unused param
     for the one in the post_main).  */
  if (s != 77)
    wstream_df_create_barrier ();
}


/***************************************************************************/
/***************************************************************************/


#ifdef _PAPI_PROFILE
void
dump_papi_counters (int eset_id)
{
  long long counters[_papi_num_events];
  char event_name[PAPI_MAX_STR_LEN];
  int i, pos = 0;
  int max_length = _papi_num_events * (PAPI_MAX_STR_LEN + 30);
  char out_buf[max_length];

  //PAPI_read (current_thread->_papi_eset[eset_id], counters);
  pos += sprintf (out_buf, "Thread %d (eset %d):", current_thread->worker_id, eset_id);
  for (i = 0; i < _papi_num_events; ++i)
    {
      PAPI_event_code_to_name (_papi_tracked_events[i], event_name);
      pos += sprintf (out_buf + pos, "\t %s %15lld", event_name, current_thread->counters[eset_id][i]);
    }
  printf ("%s\n", out_buf); fflush (stdout);
}

void
init_papi_counters (int eset_id)
{
  int retval, j;

  current_thread->_papi_eset[eset_id] = PAPI_NULL;
  if ((retval = PAPI_create_eventset (&current_thread->_papi_eset[eset_id])) != PAPI_OK)
    wstream_df_fatal ("Cannot create PAPI event set (%s)", PAPI_strerror (retval));

  if ((retval = PAPI_add_events (current_thread->_papi_eset[eset_id], _papi_tracked_events, _papi_num_events)) != PAPI_OK)
    wstream_df_fatal ("Cannot add events to set (%s)", PAPI_strerror (retval));

  for (j = 0; j < _papi_num_events; ++j)
    current_thread->counters[eset_id][j] = 0;
}


void
start_papi_counters (int eset_id)
{
  int retval;
  if ((retval = PAPI_start (current_thread->_papi_eset[eset_id])) != PAPI_OK)
    wstream_df_fatal ("Cannot sart PAPI counters (%s)", PAPI_strerror (retval));
}

void
stop_papi_counters (int eset_id)
{
  int retval, i;
  long long counters[_papi_num_events];

  if ((retval = PAPI_stop (current_thread->_papi_eset[eset_id], counters)) != PAPI_OK)
    wstream_df_fatal ("Cannot stop PAPI counters (%s)", PAPI_strerror (retval));

  for (i = 0; i < _papi_num_events; ++i)
    current_thread->counters[eset_id][i] += counters[i];
}

void
accum_papi_counters (int eset_id)
{
  int retval;

  if ((retval = PAPI_accum (current_thread->_papi_eset[eset_id], current_thread->counters[eset_id])) != PAPI_OK)
    wstream_df_fatal ("Cannot start PAPI counters (%s)", PAPI_strerror (retval));
}
#endif

/***************************************************************************/
/***************************************************************************/

/* Get the frame pointer of the current thread */
void *
__builtin_ia32_get_cfp() {
  return current_fp;
}


/* Create a new thread, with frame pointer size, and sync counter */
void *
__builtin_ia32_tcreate (size_t sc, size_t size, void *wfn)
{
  wstream_df_frame_p frame_pointer;

  __compiler_fence;

  if (posix_memalign ((void **)&frame_pointer, 64, size))
    wstream_df_fatal ("Out of memory ...");

  memset (frame_pointer, 0, size);

  frame_pointer->continuation_label_id = 0;
  frame_pointer->synchronization_counter = sc;
  frame_pointer->work_fn = (void (*) (void)) wfn;

  if (current_barrier)
    {
      current_barrier->created.counter++;
      frame_pointer->own_barrier = current_barrier;
    }

  return frame_pointer;
}


/* Decrease the synchronization counter by N.  */
static inline void
tdecrease_n (void *data, size_t n)
{
  wstream_df_frame_p fp = (wstream_df_frame_p) data;
  int sc = 0;

  if (fp->synchronization_counter != (int) n)
    sc = __sync_sub_and_fetch (&(fp->synchronization_counter), n);
  /* else the atomic sub would return 0.  This relies on the fact that
     the synchronization_counter is strictly decreasing.  */

  /* Schedule the thread if its synchronization counter reaches 0.  */
  if (sc == 0)
    {
      if (current_thread->own_next_cached_thread != NULL)
	cdeque_push_bottom (&current_thread->work_deque,
			    (wstream_df_type) current_thread->own_next_cached_thread);
      current_thread->own_next_cached_thread = fp;
    }
}

/* Decrease the synchronization counter by one.  This is not used in
   the current code generation.  Kept for compatibility with the T*
   ISA.  */
void
__builtin_ia32_tdecrease (void *data)
{
  tdecrease_n (data, 1);
}

/* Decrease the synchronization counter by N.  */
void
__builtin_ia32_tdecrease_n (void *data, size_t n)
{
  tdecrease_n (data, n);
}

/* Destroy the current thread */
void
__builtin_ia32_tend ()
{
  /* If this task belongs to a barrier, increment the exec count and
     try to pass the barrier. */
  if (current_fp->own_barrier != NULL)
    {
      current_fp->own_barrier->executed[current_thread->worker_id].counter++;
      try_pass_barrier (current_fp->own_barrier);
    }

  free (current_fp);
  current_fp = NULL;
}


/***************************************************************************/
/* Thread management and scheduling of tasks.  */
/***************************************************************************/
/***************************************************************************/

/***************************************************************************/
/* Dynamic dependence resolution.  */
/***************************************************************************/

/*
   This solver assumes that for any given stream:

   (1) all producers have the same burst == horizon
   (2) all consumers have the same horizon and
       (burst == horizon) || (burst == 0)
   (3) the consumers' horizon is an integer multiple
       of the producers' burst
*/

void
wstream_df_resolve_dependences (void *v, void *s, bool is_read_view_p)
{
  wstream_df_stream_p stream = (wstream_df_stream_p) s;
  wstream_df_view_p view = (wstream_df_view_p) v;
  wstream_df_list_p prod_queue = &stream->producer_queue;
  wstream_df_list_p cons_queue = &stream->consumer_queue;

  if (is_read_view_p == true)
    {
      /* It's either a peek view or "normal", stream-advancing.  */
      if (view->burst == 0)
	{
	  /* If this view is for a peek, then it's only going to be
	     chained as a sibling to the active_peek_chain.  */
	  view->sibling = cons_queue->active_peek_chain;
	  cons_queue->active_peek_chain = (wstream_df_list_element_p) view;
	}
      else /* Non-peeking read view.  */
	{
	  wstream_df_view_p prod_view;

	  /* If the active_peek_chain is not empty, then this current
	     task is the last missing sibling in that chain (the one
	     that implicitely TICKs the stream).  Do it
	     unconditionally as it's correctly NULL otherwise.  */
	  view->sibling = cons_queue->active_peek_chain;
	  cons_queue->active_peek_chain = NULL;

	  /* We should assume here that view->burst == view->horizon.  */
	  while (view->reached_position < view->horizon
		 && (prod_view = (wstream_df_view_p) wstream_df_list_pop (prod_queue)) != NULL)
	    {
	      void *prod_fp = prod_view->owner;
	      prod_view->data = ((char *)view->data) + view->reached_position;
	      /* Data owner is the consumer.  */
	      prod_view->owner = view->owner;
	      /* Link this view's siblings to the producer for broadcasting.  */
	      prod_view->sibling = view->sibling;

	      /* Broadcasting when multiple producers match a single
		 consumer requires that we set an overloaded
		 "reached_position" in the producer view to give it
		 the offset of the write position in the broadcast
		 chain.  This offset is only used for the sibling
		 chain.  */
	      prod_view->reached_position = view->reached_position;

	      /* Update the reached position to reflect the new
		 assigned producer block.  */
	      view->reached_position += prod_view->burst;
	      /* TDEC the producer by 1 to notify it that its
		 consumers for that view have arrived.  */
	      tdecrease_n (prod_fp, 1);
	    }
	  /* If there is not enough data scheduled to be produced, store
	     the task on the consumer queue for further matching.  */
	  if (view->reached_position < view->horizon)
	    wstream_df_list_push (cons_queue, (void *) view);
	}
    }
  else /* Write view.  */
    {
      wstream_df_view_p cons_view = (wstream_df_view_p) wstream_df_list_head (cons_queue);

      if (cons_view != NULL)
	{
	  void *prod_fp = view->owner;
	  view->data = ((char *)cons_view->data) + cons_view->reached_position;
	  view->owner = cons_view->owner;
	  view->sibling = cons_view->sibling;
	  view->reached_position = cons_view->reached_position;
	  cons_view->reached_position += view->burst;
	  tdecrease_n (prod_fp, 1);

	  if (cons_view->reached_position == cons_view->burst)
	    wstream_df_list_pop (cons_queue);
	}
      else
	wstream_df_list_push (prod_queue, (void *) view);
    }
}

/***************************************************************************/
/* Threads and scheduling.  */
/***************************************************************************/

/* Count the number of cores this process has.  */
static int
wstream_df_num_cores ()
{
  int i, cnt=0;
  cpu_set_t cs;
  CPU_ZERO (&cs);
  sched_getaffinity (getpid (), sizeof (cs), &cs);

  for (i = 0; i < MAX_NUM_CORES; i++)
    if (CPU_ISSET (i, &cs))
      cnt++;

  return cnt;
}

void *
wstream_df_worker_thread_fn (void *data)
{
  cdeque_p sched_deque;
  wstream_df_thread_p cthread = (wstream_df_thread_p) data;
  const unsigned int wid = cthread->worker_id;
  unsigned int rands = 77777 + wid * 19;
  unsigned int steal_from = 0;

  current_thread = cthread;
  current_fp = NULL;
  current_barrier = NULL;

  sched_deque = &cthread->work_deque;

  if (wid != 0)
    {
      _PAPI_INIT_CTRS (_PAPI_COUNTER_SETS);
    }

  while (true)
    {
      bool termination_p = cthread->terminate_p;
      wstream_df_frame_p fp = cthread->own_next_cached_thread;
      __compiler_fence;

      if (fp == NULL)
	fp = (wstream_df_frame_p)  (cdeque_take (sched_deque));
      else
	cthread->own_next_cached_thread = NULL;

      if (fp == NULL)
	{
	  // Cheap alternative to nrand48
	  rands = rands * 1103515245 + 12345;
	  steal_from = rands % num_workers;
	  if (__builtin_expect (steal_from != wid, 1))
	    fp = cdeque_steal (&wstream_df_worker_threads[steal_from].work_deque);
	}

      if (fp != NULL)
	{
	  current_fp = fp;
	  current_barrier = NULL;

	  _PAPI_P3B;
	  fp->work_fn ();
	  _PAPI_P3E;

	  __compiler_fence;
	}
      else if (!termination_p)
	{
	  sched_yield ();
	}
      else
	{
	  if (wid != 0)
	    {
	      _PAPI_DUMP_CTRS (_PAPI_COUNTER_SETS);
	    }
	  return NULL;
	}
    }
}

static void
start_worker (wstream_df_thread_p wstream_df_worker, int ncores)
{
  pthread_attr_t thread_attr;
  int i;

  int id = wstream_df_worker->worker_id;
  int core = id % ncores;

#ifdef _PRINT_STATS
  printf ("worker %d mapped to core %d\n", id, core);
#endif

  pthread_attr_init (&thread_attr);

  cpu_set_t cs;
  CPU_ZERO (&cs);
  CPU_SET (core, &cs);

  int errno = pthread_attr_setaffinity_np (&thread_attr, sizeof (cs), &cs);
  if (errno < 0)
    wstream_df_fatal ("pthread_attr_setaffinity_np error: %s\n", strerror (errno));

  pthread_create (&wstream_df_worker->posix_thread_id, &thread_attr,
		  wstream_df_worker_thread_fn, wstream_df_worker);

  CPU_ZERO (&cs);
  errno = pthread_getaffinity_np (wstream_df_worker->posix_thread_id, sizeof (cs), &cs);
  if (errno != 0)
    wstream_df_fatal ("pthread_getaffinity_np error: %s\n", strerror (errno));

  for (i = 0; i < CPU_SETSIZE; i++)
    if (CPU_ISSET (i, &cs) && i != core)
      wstream_df_error ("got affinity to core %d, expecting %d\n", i, core);

  if (!CPU_ISSET (core, &cs))
    wstream_df_error ("no affinity to core %d\n", core);

  pthread_attr_destroy (&thread_attr);
}

__attribute__((constructor))
void pre_main() {

  int i;

#ifdef _PAPI_PROFILE
  int retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT)
    wstream_df_fatal ("Cannot initialize PAPI library");
#endif

  /* Set workers number as the number of cores.  */
#ifndef _WSTREAM_DF_NUM_THREADS
  num_workers = wstream_df_num_cores ();
#else
  num_workers = _WSTREAM_DF_NUM_THREADS;
#endif

  if (posix_memalign ((void **)&wstream_df_worker_threads, 64,
		       num_workers * sizeof (wstream_df_thread_t)))
    wstream_df_fatal ("Out of memory ...");

  int ncores = wstream_df_num_cores ();

#ifdef _PRINT_STATS
  printf ("Creating %d workers for %d cores\n", num_workers, ncores);
#endif

  for (i = 0; i < num_workers; ++i)
    {
      cdeque_init (&wstream_df_worker_threads[i].work_deque, WSTREAM_DF_DEQUE_LOG_SIZE);
      wstream_df_worker_threads[i].terminate_p = false;
      wstream_df_worker_threads[i].worker_id = i;
      wstream_df_worker_threads[i].own_next_cached_thread = NULL;
    }

  /* Add a guard frame for the control program (in case threads catch
     up with the control program).  */
  current_thread = &wstream_df_worker_threads[0];
  current_fp = NULL;
  current_barrier = NULL;

  _PAPI_INIT_CTRS (_PAPI_COUNTER_SETS);

  __compiler_fence;

  for (i = 1; i < num_workers; ++i)
    start_worker (&wstream_df_worker_threads[i], ncores);

  wstream_df_create_barrier ();
}

__attribute__((destructor))
void post_main() {
  int i;
  void *ret;

  /* Current barrier is the last one, so it allows terminating the
     scheduler functions once it clears.  */
  wstream_df_taskwait (77);
  //current_barrier->cp_barrier = true;
  //wstream_df_release_barrier ();
  free (current_fp);
  current_fp = NULL;

  for (i = 0; i < num_workers; ++i)
    {
      wstream_df_worker_threads[i].terminate_p = true;
    }

  /* Also have this thread execute tasks once it's done.  This also
     serves in the case of 0 worker threads, to execute all on a
     single thread.  */
  wstream_df_worker_thread_fn (&wstream_df_worker_threads[0]);

  for (i = 1; i < num_workers; ++i)
    {
      pthread_join (wstream_df_worker_threads[i].posix_thread_id, &ret);
    }

  _PAPI_DUMP_CTRS (_PAPI_COUNTER_SETS);

#ifdef _PRINT_STATS
  for (i = 0; i < num_workers; ++i)
    {
      int worker_id = wstream_df_worker_threads[i].worker_id;
      printf ("worker %d executed %d tasks\n", worker_id, executed_tasks);
    }
#endif
}


/***************************************************************************/
/* Stream creation and destruction.  */
/***************************************************************************/

static inline void
init_stream (void *s, size_t element_size)
{
  ((wstream_df_stream_p) s)->producer_queue.first = NULL;
  ((wstream_df_stream_p) s)->producer_queue.last = NULL;
  ((wstream_df_stream_p) s)->producer_queue.active_peek_chain = NULL;
  ((wstream_df_stream_p) s)->consumer_queue.first = NULL;
  ((wstream_df_stream_p) s)->consumer_queue.last = NULL;
  ((wstream_df_stream_p) s)->consumer_queue.active_peek_chain = NULL;
  ((wstream_df_stream_p) s)->elem_size = element_size;
  ((wstream_df_stream_p) s)->refcount = 1;
}
/* Allocate and return an array of ARRAY_BYTE_SIZE/ELEMENT_SIZE
   streams.  */
void
wstream_df_stream_ctor (void **s, size_t element_size)
{
  if (posix_memalign (s, 64, sizeof (wstream_df_stream_t)))
    wstream_df_fatal ("Out of memory ...");
  init_stream (*s, element_size);
}

void
wstream_df_stream_array_ctor (void **s, size_t num_streams, size_t element_size)
{
  unsigned int i;

  for (i = 0; i < num_streams; ++i)
    {
      if (posix_memalign (&s[i], 64, sizeof (wstream_df_stream_t)))
	wstream_df_fatal ("Out of memory ...");
      init_stream (s[i], element_size);
    }
}

void __builtin_ia32_tick (void *, size_t);
static inline void
force_empty_queues (void *s)
{
  wstream_df_stream_p stream = (wstream_df_stream_p) s;
  wstream_df_list_p prod_queue = &stream->producer_queue;
  wstream_df_list_p cons_queue = &stream->consumer_queue;
  wstream_df_view_p view;

  if (wstream_df_list_head (cons_queue) != NULL)
    {
      /* It may be possible for concurrent resolve_dependences calls
	 to miss each other between producers and consumers.  Match
	 any residual views in queues now.  */
      while ((view = (wstream_df_view_p) wstream_df_list_head (cons_queue)) != NULL)
	{
	  wstream_df_list_pop (cons_queue);
	  wstream_df_resolve_dependences ((void *) view, s, true);

	  //wstream_df_error ("Late resolution ...");

	  if (wstream_df_list_head (cons_queue) != NULL
	      && wstream_df_list_head (prod_queue) == NULL)
	    {
	      wstream_df_error ("Leftover consumers at stream destruction");
	      break;
	    }
	}
    }
  while ((view = (wstream_df_view_p) wstream_df_list_head (prod_queue)) != NULL)
    {
      __builtin_ia32_tick (stream, view->burst / stream->elem_size);
    }
}

static inline void
dec_stream_ref (void *s)
{
  wstream_df_stream_p stream = (wstream_df_stream_p) s;

  int refcount = __sync_sub_and_fetch (&stream->refcount, 1);

  if (refcount == 0)
    {
      force_empty_queues (s);
      free (s);
    }
#if 0
  int refcount = stream->refcount;

  // debug --
#ifdef _WSTREAM_DF_DEBUG
  if (refcount < 1)
    wstream_df_fatal ("Stream destructor called for a refcount < 1 (refcount == %d)", refcount);
#endif

  if (refcount > 1)
    refcount = __sync_sub_and_fetch (&stream->refcount, 1);
  else
    {
#ifdef _WSTREAM_DF_DEBUG
      refcount = __sync_sub_and_fetch (&stream->refcount, 1);

      if (refcount != 0)
	wstream_df_fatal ("Stream destructor called for a refcount < 1 (refcount == %d)", refcount);
#else
      refcount = 0;
#endif
    }

  if (refcount == 0)
    {
      force_empty_queues (s);
      free (s);
    }
#endif
}

/* Deallocate the array of streams S.  */
void
wstream_df_stream_dtor (void **s, size_t num_streams)
{
  unsigned int i;

  if (num_streams == 1)
    {
      dec_stream_ref ((void *)s);
    }
  else
    {
      for (i = 0; i < num_streams; ++i)
	dec_stream_ref (s[i]);
    }
}

/* Take an additional reference on stream(s) S.  */
void
wstream_df_stream_reference (void *s, size_t num_streams)
{
  if (num_streams == 1)
    {
      wstream_df_stream_p stream = (wstream_df_stream_p) s;
      __sync_add_and_fetch (&stream->refcount, 1);
    }
  else
    {
      unsigned int i;

      for (i = 0; i < num_streams; ++i)
	{
	  wstream_df_stream_p stream = (wstream_df_stream_p) ((void **) s)[i];
	  __sync_add_and_fetch (&stream->refcount, 1);
	}
    }
}

/***************************************************************************/
/* Runtime extension for arrays of views and DF broadcast.  */
/***************************************************************************/

void
wstream_df_resolve_n_dependences (size_t n, void *v, void *s, bool is_read_view_p)
{
  wstream_df_view_p dummy_view = (wstream_df_view_p) v;
  unsigned int i;

  for (i = 0; i < n; ++i)
    {
      wstream_df_stream_p stream = ((wstream_df_stream_p *) s)[i];
      wstream_df_view_p view = &((wstream_df_view_p) dummy_view->next)[i];

      /* Data position for consumers.  */
      view->data = (is_read_view_p) ?
	(void *)(((char *) dummy_view->data) + i * dummy_view->horizon) : NULL;

      /* Only connections with the same burst are allowed for now.  */
      view->burst = dummy_view->burst;
      view->horizon = dummy_view->horizon;
      view->owner = dummy_view->owner;

      wstream_df_resolve_dependences ((void *) view, (void *) stream, is_read_view_p);
    }
}

static inline void
broadcast (void *v)
{
  wstream_df_view_p prod_view = (wstream_df_view_p) v;
  wstream_df_view_p cons_view = prod_view;

  size_t offset = prod_view->reached_position;
  size_t burst = prod_view->burst;
  void *base_addr = prod_view->data;

  while ((cons_view = cons_view->sibling) != NULL)
    {
      memcpy (((char *)cons_view->data) + offset, base_addr, burst);
      tdecrease_n ((void *) cons_view->owner, burst);
    }
}

void
__builtin_ia32_broadcast (void *v)
{
  broadcast (v);
}

/* Decrease the synchronization counter by N.  */
void
__builtin_ia32_tdecrease_n_vec (size_t num, void *data, size_t n)
{
  unsigned int i;

  for (i = 0; i < num; ++i)
    {
      wstream_df_view_p dummy_view = (wstream_df_view_p) data;
      wstream_df_view_p view = &((wstream_df_view_p) dummy_view->next)[i];
      wstream_df_frame_p fp = (wstream_df_frame_p) view->owner;

      if (view->sibling)
	broadcast ((void *) view);
      tdecrease_n ((void *) fp, n);
    }
}

void
__builtin_ia32_tick (void *s, size_t burst)
{
  wstream_df_stream_p stream = (wstream_df_stream_p) s;
  wstream_df_list_p cons_queue = &stream->consumer_queue;
  wstream_df_view_p cons_view;

  /* ASSERT that cons_view->active_peek_chain != NULL !! Otherwise
     this requires passing the matching producers a fake consumer view
     where to write at least during the producers' execution ... */
  if (cons_queue->active_peek_chain == NULL)
    {
      /* Normally an error, but for OMPSs expansion, we need to allow
	 it.  */
      /*  wstream_df_fatal ("TICK (%d) matches no PEEK view, which is unsupported at this time.",
	  burst * stream->elem_size);
      */
      /* Allocate a fake view, with a fake data block, that allows to
	 keep the same dependence resolution algorithm.  */
      size_t size = sizeof (wstream_df_view_t) + burst * stream->elem_size;
      if (posix_memalign ((void **)&cons_view, 64, size))
	wstream_df_fatal ("Out of memory ...");
      memset (cons_view, 0, size);

      cons_view->horizon = burst * stream->elem_size;
      cons_view->burst = cons_view->horizon;
      cons_view->owner = cons_view;
      cons_view->data = ((char *) cons_view) + sizeof (wstream_df_view_t);
    }
  else
    {
      /* TICKing mostly means flushing the active_peek_chain to the main
	 consumer queue, then trying to resolve dependences.  */
      cons_view = (wstream_df_view_p) cons_queue->active_peek_chain;
      cons_queue->active_peek_chain = cons_view->sibling;
      cons_view->sibling = NULL;
      cons_view->burst = cons_view->horizon;

      /* ASSERT that burst == cons_view->horizon !! */
      if (burst * stream->elem_size != cons_view->horizon)
	wstream_df_fatal ("TICK burst of %d elements does not match the burst %d of preceding PEEK operations.",
		    burst, cons_view->horizon / stream->elem_size);
    }

  wstream_df_resolve_dependences ((void *) cons_view, s, true);
}


