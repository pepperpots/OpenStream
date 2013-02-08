#ifndef _WSTREAM_DF_H_
#define _WSTREAM_DF_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fibers.h"
#include "alloc.h"
#include "cdeque.h"
#include "config.h"
#include "list.h"
#include "trace.h"

#define STEAL_TYPE_UNKNOWN 0
#define STEAL_TYPE_PUSH 1
#define STEAL_TYPE_STEAL 2

/* Create a new thread, with frame pointer size, and sync counter */
extern void *__builtin_ia32_tcreate (size_t, size_t, void *, bool);
/* Decrease the synchronization counter by one */
extern void __builtin_ia32_tdecrease (void *, bool);
/* Decrease the synchronization counter by one */
extern void __builtin_ia32_tdecrease_n (void *, size_t, bool);
extern void __builtin_ia32_tdecrease_n_vec (size_t, void *, size_t, bool);
/* Destroy (free) the current thread */
extern void __builtin_ia32_tend (void *);

/* Suspend the execution of the current task until all children tasks
   spawned up to this point have completed.  When the las spawned task
   has a lastprivate clause (__builtin_ia32_tcreate_lp), only this
   task is awaited, but a subsequent call would wait for all other
   tasks.  */
extern void wstream_df_taskwait ();

/* Allocate and return an array of streams.  */
extern void wstream_df_stream_ctor (void **, size_t);
extern void wstream_df_stream_array_ctor (void **, size_t, size_t);
/* Decrement reference counter on streams in the array and deallocate
   streams upon reaching 0.  */
extern void wstream_df_stream_dtor (void **, size_t);
/* Add a reference to a stream when passing it as firstprivate to a
   task.  */
extern void wstream_df_stream_reference (void *, size_t);


/***************************************************************************/
/* Data structures for T*.  */
/***************************************************************************/

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


typedef struct wstream_df_frame
{
  int synchronization_counter;
  int size;
  void (*work_fn) (void *);
  struct barrier *own_barrier;

  int steal_type;
  int last_owner;
  int bytes_cpu[MAX_CPUS];

  /* Variable size struct */
  //char buf [];
} wstream_df_frame_t, *wstream_df_frame_p;

typedef struct barrier
{
  int barrier_counter_executed __attribute__((aligned (64)));
  int barrier_counter_created __attribute__((aligned (64)));
  bool barrier_unused;
  ws_ctx_t continuation_context;

  struct barrier *save_barrier;
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
  unsigned int worker_id;

  cdeque_t work_deque __attribute__((aligned (64)));
  wstream_df_frame_p own_next_cached_thread __attribute__((aligned (64)));

#if !NO_SLAB_ALLOCATOR
  slab_cache_t slab_cache;
#endif

  unsigned int rands;

#if ALLOW_PUSHES
  wstream_df_frame_p pushed_threads[NUM_PUSH_SLOTS] __attribute__((aligned (64)));
#endif

  barrier_p swap_barrier;
  void *current_stack; // BUG in swap/get context: stack is not set
#ifdef _PAPI_PROFILE
  int _papi_eset[16];
  long long counters[16][_papi_num_events];
#endif

#ifdef WQUEUE_PROFILE
  unsigned long long steals_fails;
  unsigned long long steals_owncached;
  unsigned long long steals_ownqueue;
  unsigned long long steals_samel2;
  unsigned long long steals_samel3;
  unsigned long long steals_remote;
#ifdef ALLOW_PUSHES
  unsigned long long steals_pushed;
  unsigned long long pushes_samel2;
  unsigned long long pushes_samel3;
  unsigned long long pushes_remote;
  unsigned long long pushes_fails;
#endif
  unsigned long long bytes_l1;
  unsigned long long bytes_l2;
  unsigned long long bytes_l3;
  unsigned long long bytes_rem;
  unsigned long long tasks_created;
  unsigned long long tasks_executed;
#endif

#if ALLOW_WQEVENT_SAMPLING
  worker_state_change_t events[MAX_WQEVENT_SAMPLES];
  unsigned int num_events;
  unsigned int previous_state_idx;
#endif
} wstream_df_thread_t, *wstream_df_thread_p;

#endif
