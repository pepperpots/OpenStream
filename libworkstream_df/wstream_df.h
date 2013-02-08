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
#include "profiling.h"

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

#if ALLOW_PUSHES
  #define WSTREAM_DF_THREAD_PUSH_FIELDS wstream_df_frame_p pushed_threads[NUM_PUSH_SLOTS] __attribute__((aligned (64)))
#else
  #define WSTREAM_DF_THREAD_PUSH_FIELDS
#endif

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

  unsigned int rands;

  WSTREAM_DF_THREAD_SLAB_FIELDS;
  WSTREAM_DF_THREAD_PUSH_FIELDS;
  WSTREAM_DF_THREAD_PAPI_FIELDS;
  WSTREAM_DF_THREAD_WQUEUE_PROFILE_FIELDS;
  WSTREAM_DF_THREAD_EVENT_SAMPLING_FIELDS;

  barrier_p swap_barrier;
  void *current_stack; // BUG in swap/get context: stack is not set
} wstream_df_thread_t, *wstream_df_thread_p;

#endif
