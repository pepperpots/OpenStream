#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//#define _WSTREAM_DF_DEBUG 1
//#define _PAPI_PROFILE
#include "papi-defs.h"

#include "wstream_df.h"
#include "error.h"
#include "cdeque.h"
#include "alloc.h"
#include "fibers.h"

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
  int synchronization_counter;
  int size;
  void (*work_fn) (void *);
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


typedef struct barrier
{
  int barrier_counter_executed __attribute__((aligned (64)));
  int barrier_counter_created __attribute__((aligned (64)));
  bool barrier_unused;
  ws_ctx_t continuation_context;

} barrier_t, *barrier_p;

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
  int worker_id;

  cdeque_t work_deque __attribute__((aligned (64)));
  wstream_df_frame_p own_next_cached_thread __attribute__((aligned (64)));

  barrier_p swap_barrier;
  void *current_stack; // BUG in swap/get context: stack is not set
#ifdef _PAPI_PROFILE
  int _papi_eset[16];
  long long counters[16][_papi_num_events];
#endif
} wstream_df_thread_t, *wstream_df_thread_p;

/***************************************************************************/
/***************************************************************************/
/* The current frame pointer, thread data, barrier and saved barrier
   for lastprivate implementation, are stored here in TLS.  */
static __thread wstream_df_thread_p current_thread = NULL;
static __thread wstream_df_frame_p current_fp = NULL;
static __thread barrier_p current_barrier = NULL;
static __thread barrier_p save_current_barrier = NULL;

static wstream_df_thread_p wstream_df_worker_threads;
static int num_workers;

/*************************************************************************/
/*******             BARRIER/SYNC Handling                         *******/
/*************************************************************************/

static void worker_thread ();

static inline barrier_p
wstream_df_create_barrier ()
{
  barrier_p barrier;

  wstream_alloc (&barrier, 64, sizeof (barrier_t));
  memset (barrier, 0, sizeof (barrier_t));

  /* Add one guard elemment, it will be matched by one increment of
     the exec counter once the barrier is ready.  */
  barrier->barrier_counter_created = 1;
  return barrier;
}

static inline __attribute__((__optimize__("O1")))
void
try_pass_barrier (barrier_p bar)
{
  int exec = __sync_add_and_fetch (&bar->barrier_counter_executed, 1);

  if (bar->barrier_counter_created == exec)
    {
      if (bar->barrier_unused == true)
	wstream_free (bar, sizeof (barrier_t));
      else
	{
	  ws_ctx_t ctx;

	  if (ws_swapcontext (&ctx, &bar->continuation_context) == -1)
	    wstream_df_fatal ("Cannot swap contexts when passing barrier.");

	  /* This swap is "final" so this thread will never be resumed.
	     The stack is collected by the continuation.  */
	}
    }
}

__attribute__((__optimize__("O1")))
void
wstream_df_taskwait ()
{
  /* Save on stack all thread-local variables.  The contents will be
     clobbered once another context runs on this thread, but the stack
     is saved.  */
  void *save_stack = current_thread->current_stack;
  wstream_df_frame_p save_frame = current_fp;
  barrier_p bar = current_barrier;
  barrier_p save_bar = save_current_barrier;

  /* If no barrier is associated, then just return, no sync needed.  */
  if (bar == NULL)
    return;

  /* If a barrier is present, but no tasks are associated to the
     barrier, then we have an error.  */
  if (bar->barrier_counter_created == 1)
    wstream_df_fatal ("Barrier created without associated tasks.");

  /* If the barrier si ready to pass, missing only the guard element,
     then just pass through.  */
  if (bar->barrier_counter_created != bar->barrier_counter_executed + 1)
    {
      ws_ctx_t ctx;
      void *stack;

      /* Reset current_barrier on this thread, the barrier reference is
	 kept in the stack-local variable BAR.  */
      current_barrier = NULL;

      /* Build a new context to execute the scheduler function after the
	 swap.  Allocate a new stack and set the continuation link to NULL
	 (in general, it is not possible to know what the continuation of
	 this scheduler will be, so leave NULL then set when a barrier
	 passes to the barrier's continuation).  */

      wstream_alloc(&stack, 64, WSTREAM_STACK_SIZE);
      ws_prepcontext (&ctx, stack, WSTREAM_STACK_SIZE, worker_thread);
      ws_prepcontext (&bar->continuation_context, stack, WSTREAM_STACK_SIZE, worker_thread);

      current_thread->swap_barrier = bar;
      current_thread->current_stack = stack;

      if (ws_swapcontext (&bar->continuation_context, &ctx) == -1)
	wstream_df_fatal ("Cannot swap contexts at taskwait.");

      /* When this context is reactivated, we must deallocate the stack of
	 the context that was swapped out (can only happen in TEND).  The
	 stack-stored variable BAR should have been preserved even if the
	 thread-local "current_barrier" has not.  */
      wstream_free (current_thread->current_stack, WSTREAM_STACK_SIZE);
    }

  wstream_free (bar, sizeof (barrier_t));
  /* Restore thread-local variables.  */
  current_thread->current_stack = save_stack;
  current_fp = save_frame;
  current_barrier = save_bar;  /* If this is a LP sync, restore barrier.  */
  save_current_barrier = NULL;
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

/* Create a new thread, with frame pointer size, and sync counter */
void *
__builtin_ia32_tcreate (size_t sc, size_t size, void *wfn, bool has_lp)
{
  wstream_df_frame_p frame_pointer;

  __compiler_fence;

  wstream_alloc(&frame_pointer, 64, size);
  //memset (frame_pointer, 0, size);

  frame_pointer->synchronization_counter = sc;
  frame_pointer->size = size;
  frame_pointer->work_fn = (void (*) (void *)) wfn;

  if (has_lp)
    {
      save_current_barrier = current_barrier;
      current_barrier = wstream_df_create_barrier ();
    }

  if (current_barrier == NULL)
    current_barrier = wstream_df_create_barrier ();

  current_barrier->barrier_counter_created++;
  frame_pointer->own_barrier = current_barrier;

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
  barrier_p bar = current_fp->own_barrier;

  /* If this task spawned some tasks within a barrier, it needs to
     ensure the barrier gets collected.  */
  if (current_barrier != NULL)
    {
      if (current_barrier->barrier_counter_created == 1)
	wstream_df_fatal ("Barrier created without associated tasks.");

      current_barrier->barrier_unused = true;
      try_pass_barrier (current_barrier);
      current_barrier = NULL;
    }

  wstream_free (current_fp, current_fp->size);
  current_fp = NULL;

  /* If this task belongs to a barrier, increment the exec count and
     try to pass the barrier.  */
  if (bar != NULL)
    try_pass_barrier (bar);
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

static __attribute__((__optimize__("O1"))) void
worker_thread (void)
{
  current_fp = NULL;
  current_barrier = NULL;
  save_current_barrier = NULL;
  unsigned int rands = 77777 + current_thread->worker_id * 19;
  unsigned int steal_from = 0;

  /* Enable barrier passing if needed.  */
  if (current_thread->swap_barrier != NULL)
    {
      barrier_p bar = current_thread->swap_barrier;
      current_thread->swap_barrier = NULL;
      try_pass_barrier (bar);
    }


  if (current_thread->worker_id != 0)
    {
      _PAPI_INIT_CTRS (_PAPI_COUNTER_SETS);
    }

  while (true)
    {
      wstream_df_frame_p fp = current_thread->own_next_cached_thread;
      __compiler_fence;

      if (fp == NULL)
	fp = (wstream_df_frame_p)  (cdeque_take (&current_thread->work_deque));
      else
	current_thread->own_next_cached_thread = NULL;

      if (fp == NULL)
	{
	  // Cheap alternative to nrand48
	  rands = rands * 1103515245 + 12345;
	  steal_from = rands % num_workers;
	  if (__builtin_expect (steal_from != current_thread->worker_id, 1))
	    fp = cdeque_steal (&wstream_df_worker_threads[steal_from].work_deque);
	}

      if (fp != NULL)
	{
	  current_fp = fp;
	  current_barrier = NULL;

	  _PAPI_P3B;
	  fp->work_fn (fp);
	  _PAPI_P3E;

	  __compiler_fence;
	}
      else
	{
#ifndef _WS_NO_YIELD_SPIN
	  sched_yield ();
#endif
	}
    }
}

void *
wstream_df_worker_thread_fn (void *data)
{
  if (((wstream_df_thread_p) data)->worker_id != 0)
    {
      wstream_init_alloc();
    }
  current_thread = ((wstream_df_thread_p) data);
  worker_thread ();
  return NULL;
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
  pthread_attr_setdetachstate (&thread_attr, PTHREAD_CREATE_DETACHED);

  cpu_set_t cs;
  CPU_ZERO (&cs);
  CPU_SET (core, &cs);

  int errno = pthread_attr_setaffinity_np (&thread_attr, sizeof (cs), &cs);
  if (errno < 0)
    wstream_df_fatal ("pthread_attr_setaffinity_np error: %s\n", strerror (errno));

  void *stack;
  wstream_alloc(&stack, 64, WSTREAM_STACK_SIZE);
  errno = pthread_attr_setstack (&thread_attr, stack, WSTREAM_STACK_SIZE);
  wstream_df_worker->current_stack = stack;

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
void pre_main()
{
  int i;
#ifdef _PAPI_PROFILE
  int retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT)
    wstream_df_fatal ("Cannot initialize PAPI library");
#endif

  wstream_init_alloc();

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
      wstream_df_worker_threads[i].worker_id = i;
      wstream_df_worker_threads[i].own_next_cached_thread = NULL;
      wstream_df_worker_threads[i].swap_barrier = NULL;
    }

  /* Add a guard frame for the control program (in case threads catch
     up with the control program).  */
  current_thread = &wstream_df_worker_threads[0];
  current_fp = NULL;
  current_barrier = NULL;
  save_current_barrier = NULL;

  _PAPI_INIT_CTRS (_PAPI_COUNTER_SETS);

  __compiler_fence;

  for (i = 1; i < num_workers; ++i)
    start_worker (&wstream_df_worker_threads[i], ncores);
}

__attribute__((destructor))
void post_main() {
  int i;
  void *ret;

  /* Current barrier is the last one, so it allows terminating the
     scheduler functions and exiting once it clears.  */
  wstream_df_taskwait ();

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
  wstream_alloc(s, 64, sizeof (wstream_df_stream_t));
  init_stream (*s, element_size);
}

void
wstream_df_stream_array_ctor (void **s, size_t num_streams, size_t element_size)
{
  unsigned int i;

  for (i = 0; i < num_streams; ++i)
    {
      wstream_alloc(&s[i], 64, sizeof (wstream_df_stream_t));
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
      wstream_free(s, sizeof (wstream_df_stream_t));
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
      wstream_free(s, sizeof (wstream_df_stream_t));
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
      wstream_alloc(&cons_view, 64, size);
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


