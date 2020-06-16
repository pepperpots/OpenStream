#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdalign.h>
#include <stdint.h>


#include "config.h"
#include "hwloc.h"
#include "trace.h"
#include "wstream_df.h"
#include "profiling.h"
#include "arch.h"
#include "work_distribution.h"
#include "tsc.h"
#include "numa.h"
#include "alloc.h"
#include "reuse.h"
#include "prng.h"
#include "hwloc-support.h"

#ifdef DEPENDENCE_AWARE_ALLOC
	#error "Obsolete option 'dependence-aware allocation' enabled"
#endif

/***************************************************************************/
/***************************************************************************/
/* The current frame pointer, thread data, barrier and saved barrier
   for lastprivate implementation, are stored here in TLS.  */
__thread wstream_df_thread_p current_thread = NULL;
static __thread barrier_p current_barrier = NULL;

static wstream_df_thread_p* wstream_df_worker_threads;
unsigned wstream_num_workers;

void __built_in_wstream_df_dec_frame_ref(wstream_df_frame_p fp, size_t n);

/*************************************************************************/
/*******             BARRIER/SYNC Handling                         *******/
/*************************************************************************/

static void worker_thread ();
#if ALLOW_WQEVENT_SAMPLING
static void trace_signal_handler(int sig);
#endif

static inline void wstream_free_frame(wstream_df_frame_p fp)
{
  slab_free(current_thread->slab_cache, fp);
  trace_tdestroy(current_thread, fp);
}

int wstream_self(void)
{
  wstream_df_thread_p cthread = current_thread;

  if (cthread == NULL)
    wstream_df_fatal ("Current_thread lost...");

  return cthread->worker_id;
}

static inline barrier_p
wstream_df_create_barrier ()
{
  barrier_p barrier;

  barrier = slab_alloc (current_thread, current_thread->slab_cache, sizeof (barrier_t));
  memset (barrier, 0, sizeof (barrier_t));

  /* Add one guard elemment, it will be matched by one increment of
     the exec counter once the barrier is ready.  */
  barrier->barrier_counter_created = 1;
  return barrier;
}

__attribute__((__optimize__("O1")))
static inline void
try_pass_barrier (barrier_p bar)
{
  int exec = __sync_add_and_fetch (&bar->barrier_counter_executed, 1);

  if (bar->barrier_counter_created == exec)
    {
      if (bar->barrier_unused == true)
	slab_free (current_thread->slab_cache, bar);
      else
	{
	  wstream_df_thread_p cthread = current_thread;
	  trace_task_exec_end(cthread, cthread->current_frame);
	  cthread->current_work_fn = NULL;
	  cthread->current_frame = NULL;
	  trace_state_change(cthread, WORKER_STATE_RT_INIT);
	  wqueue_counters_enter_runtime(cthread);
	  inc_wqueue_counter(&cthread->tasks_executed, 1);


	  if (ws_setcontext (&bar->continuation_context) == -1)
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
  wstream_df_thread_p cthread = current_thread;

  /* Save on stack all thread-local variables.  The contents will be
     clobbered once another context runs on this thread, but the stack
     is saved.  */
  void *save_stack = cthread->current_stack;
  barrier_p cbar = current_barrier;
  barrier_p save_bar = NULL;
  void* save_current_work_fn = cthread->current_work_fn;
  void* save_current_frame = cthread->current_frame;

  wqueue_counters_enter_runtime(cthread);

  /* If no barrier is associated, then just return, no sync needed.  */
  if (cbar == NULL)
    return;
  else
    save_bar = cbar->save_barrier;

  /* If a barrier is present, but no tasks are associated to the
     barrier, then we have an error.  */
  if (cbar->barrier_counter_created == 1)
    wstream_df_fatal ("Barrier created without associated tasks.");

  /* If the barrier si ready to pass, missing only the guard element,
     then just pass through.  */
  if (cbar->barrier_counter_created != cbar->barrier_counter_executed + 1)
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

      stack = slab_alloc(cthread, cthread->slab_cache, WSTREAM_STACK_SIZE);
      ws_prepcontext (&ctx, stack, WSTREAM_STACK_SIZE, worker_thread);
      ws_prepcontext (&cbar->continuation_context, NULL, WSTREAM_STACK_SIZE, NULL);

      cthread->swap_barrier = cbar;
      cthread->current_stack = stack;
      cthread->current_work_fn = NULL;
      cthread->current_frame = NULL;

      if (ws_swapcontext (&cbar->continuation_context, &ctx) == -1)
	wstream_df_fatal ("Cannot swap contexts at taskwait.");

      /* Restore local copy of current thread from TLS when resuming
	 execution.  This may not be the same worker thread as before
	 swapping contexts.  */
      cthread = current_thread;

      /* When this context is reactivated, we must deallocate the stack of
	 the context that was swapped out (can only happen in TEND).  The
	 stack-stored variable BAR should have been preserved even if the
	 thread-local "current_barrier" has not.  */
      /* FIXME: WHY DOES THIS CAUSE SEGFAULTS? */
      //wstream_free (cthread->slab_cache, cthread->current_stack);
    }

  slab_free (cthread->slab_cache, cbar);
  /* Restore thread-local variables.  */
  cthread->current_stack = save_stack;
  current_barrier = save_bar;  /* If this is a LP sync, restore barrier.  */
  cthread->current_work_fn = save_current_work_fn;
  cthread->current_frame = save_current_frame;
}

/***************************************************************************/
/***************************************************************************/

static inline void *align_up(void *addr, size_t alignment) {
  uintptr_t ptr = (uintptr_t)addr;
  ptr = (ptr + alignment - 1) & (~(alignment - 1));
  return (void *)ptr;
}

/* Create a new thread, with frame pointer size, and sync counter */
void *__builtin_ia32_tcreate(size_t sc, size_t size, void *wfn, bool has_lp) {
  wstream_df_frame_p frame_pointer;
  barrier_p cbar = current_barrier;
  wstream_df_thread_p cthread = current_thread;
#if WQUEUE_PROFILE
  // Allocating more place for profiling data
  size_t extra_size =
      num_numa_nodes * sizeof(*frame_pointer->bytes_prematch_nodes) +
      alignof(int) +
      num_numa_nodes * sizeof(*frame_pointer->bytes_reuse_nodes) +
      alignof(int) +
      wstream_num_workers * sizeof(*frame_pointer->bytes_cpu_in) +
      alignof(int) +
      wstream_num_workers * sizeof(*frame_pointer->cache_misses) +
      alignof(long long) +
      wstream_num_workers * sizeof(*frame_pointer->bytes_cpu_ts) +
      alignof(long long);
#else
  size_t extra_size = 0;
#endif

  __compiler_fence;

  wqueue_counters_enter_runtime(cthread);
  trace_state_change(cthread, WORKER_STATE_RT_TCREATE);
  trace_tcreate(cthread, NULL);

#if ALLOW_WQEVENT_SAMPLING
  int curr_idx = cthread->num_events - 1;
#endif // ALLOW_WQEVENT_SAMPLING
  frame_pointer = slab_alloc(cthread, cthread->slab_cache, size + extra_size);

  //  printf("F+ Allocating %p\n", frame_pointer);

  frame_pointer->synchronization_counter = sc + 1;
  frame_pointer->size = size;
  frame_pointer->work_fn = (void (*)(void *))wfn;
  frame_pointer->refcount = 1;
  frame_pointer->input_view_chain = NULL;
  frame_pointer->output_view_chain = NULL;

#if WQUEUE_PROFILE

  // The slab alocator is expensive (mainly spinlock), so reducing the calls to
  // the allocator to one per task creation is necessary for fine grained
  // programs.

  // Compute profiling array address within the extra space allocated after the
  // frame
  char *fp = (char *)frame_pointer;
  fp += size;
  memset(fp, 0, extra_size);

  fp = align_up(fp, alignof(int));
  frame_pointer->bytes_prematch_nodes = (int *)fp;
  fp += num_numa_nodes * sizeof(int);

  frame_pointer->bytes_reuse_nodes = (int *)fp;
  fp += num_numa_nodes * sizeof(int);

  frame_pointer->bytes_cpu_in = (int *)fp;
  fp += wstream_num_workers * sizeof(int);

  fp = align_up(fp, alignof(long long));
  frame_pointer->bytes_cpu_ts = (long long *)fp;
  fp += wstream_num_workers * sizeof(long long);

  frame_pointer->cache_misses = (long long *)fp;

  frame_pointer->steal_type = STEAL_TYPE_UNKNOWN;
  frame_pointer->last_owner = cthread->worker_id;
  frame_pointer->creation_timestamp = rdtsc() - cthread->tsc_offset;
  frame_pointer->cache_misses[cthread->worker_id] = mem_cache_misses(cthread);
  frame_pointer->dominant_input_data_node_id = -1;
  frame_pointer->dominant_input_data_size = 0;
  frame_pointer->dominant_prematch_data_node_id = -1;
  frame_pointer->dominant_prematch_data_size = 0;

#endif

#if ALLOW_WQEVENT_SAMPLING
  cthread->events[curr_idx].tcreate.frame = (uint64_t)frame_pointer;
#endif

  inc_wqueue_counter(&cthread->tasks_created, 1);

  if (has_lp) {
    barrier_p temp_bar = cbar;
    cbar = wstream_df_create_barrier();
    current_barrier = cbar;
    cbar->save_barrier = temp_bar;
  }

  if (cbar == NULL) {
    cbar = wstream_df_create_barrier();
    current_barrier = cbar;
  }

  cbar->barrier_counter_created++;
  frame_pointer->own_barrier = cbar;

  trace_state_restore(cthread);

  return frame_pointer;
}

void get_max_worker(int *bytes_cpu, unsigned int num_workers,
                    unsigned int *pmax_worker, int *pmax_data) {
  unsigned int max_worker = 0;
  int max_data = 0;

  for (unsigned int worker_id = 0; worker_id < num_workers; worker_id++) {

    if (bytes_cpu[worker_id] > max_data) {
      max_data = bytes_cpu[worker_id];
      max_worker = worker_id;
    }
  }

  *pmax_worker = max_worker;
  *pmax_data = max_data;
}

void get_max_worker_same_node(int *bytes_cpu, unsigned int num_workers,
                              unsigned int *pmax_worker, int *pmax_data,
                              int numa_node_id) {
  unsigned int max_worker = 0;
  int max_data = 0;

  for (unsigned int worker_id = 0; worker_id < num_workers; worker_id++) {

    if (bytes_cpu[worker_id] > max_data) {
      wstream_df_thread_p worker = wstream_df_worker_threads[worker_id];

      if (worker->numa_node->id == numa_node_id) {
        max_data = bytes_cpu[worker_id];
        max_worker = worker_id;
      }
    }
  }
  *pmax_worker = max_worker;
  *pmax_data = max_data;
}

void update_numa_nodes_of_views(wstream_df_thread_p cthread, wstream_df_frame_p fp)
{
  for(struct wstream_df_view* v = fp->input_view_chain; v; v = v->view_chain_next)
    if(v->data)
      slab_update_numa_node_of_if_fresh(v->data, cthread, 1);
}

/* Decrease the synchronization counter by N.  */
static inline void
tdecrease_n (void *data, size_t n, bool is_write)
{

  wstream_df_frame_p fp = (wstream_df_frame_p) data;
  wstream_df_thread_p cthread = current_thread;

  trace_state_change(cthread, WORKER_STATE_RT_TDEC);

  if(is_write) {
#if WQUEUE_PROFILE
    inc_wqueue_counter(&fp->bytes_cpu_in[cthread->worker_id], n);
    set_wqueue_counter_if_zero(&fp->bytes_cpu_ts[cthread->worker_id], rdtsc() - cthread->tsc_offset);

    if (fp->cache_misses[cthread->worker_id] == 0)
      fp->cache_misses[cthread->worker_id] = mem_cache_misses(cthread);
#endif
  }

  int sc = 0;

  if (fp->synchronization_counter != (int) n)
    sc = __sync_sub_and_fetch (&(fp->synchronization_counter), n);

  /* else the atomic sub would return 0.  This relies on the fact that
     the synchronization_counter is strictly decreasing.  */

  /* Schedule the thread if its synchronization counter reaches 0.  */
  if (sc == 0)
    {
      if (fp->work_fn == (void *) 1)
	{
	  __builtin_ia32_tend (fp);
	  trace_state_restore(cthread);
	  return;
	}

      update_numa_nodes_of_views(cthread, fp);

#if ALLOW_WQEVENT_SAMPLING
      fp->last_owner = cthread->worker_id;
      fp->steal_type = STEAL_TYPE_PUSH;
      fp->ready_timestamp = rdtsc() - cthread->tsc_offset;
#endif

#if ALLOW_PUSHES
      int target_worker;
      /* Check whether the frame should be pushed somewhere else */
      int beneficial =
          work_push_beneficial(fp, cthread, wstream_num_workers,
                               wstream_df_worker_threads, &target_worker);

#ifdef PUSH_ONLY_IF_NOT_STOLEN_AND_CACHE_EMPTY
      int curr_stolen = (cthread->current_frame &&
			 ((wstream_df_frame_p)cthread->current_frame)->steal_type == STEAL_TYPE_STEAL);

#if !DISABLE_WQUEUE_LOCAL_CACHE
      if (curr_stolen && !cthread->own_next_cached_thread)
        beneficial = 0;
#endif // !DISABLE_WQUEUE_LOCAL_CACHE
#endif // PUSH_ONLY_IF_NOT_STOLEN_AND_CACHE_EMPTY

      if(beneficial)
	{
	  if(work_try_push(fp, target_worker, cthread, wstream_df_worker_threads))
	    {
	      trace_state_restore(cthread);
	      return;
	    }
	}
#endif // ALLOW_PUSHES
#if DISABLE_WQUEUE_LOCAL_CACHE
	cdeque_push_bottom (&cthread->work_deque,
			    (wstream_df_type) fp);
#else // !DISABLE_WQUEUE_LOCAL_CACHE
      if (cthread->own_next_cached_thread != NULL)
	cdeque_push_bottom (&cthread->work_deque,
			    (wstream_df_type) cthread->own_next_cached_thread);
      cthread->own_next_cached_thread = fp;
#endif // DISABLE_WQUEUE_LOCAL_CACHE
    }

  trace_state_restore(cthread);
}

/* Called when all calls to resolve_dependences have been issued. The
 * final decrementation prevents the task from running in case there
 * are some variadic views with 0 references left.*/
void __builtin_finish_resdep(void* pfp)
{
	tdecrease_n (pfp, 1, false);
}

/* Decrease the synchronization counter by one.  This is not used in
   the current code generation.  Kept for compatibility with the T*
   ISA.  */
void
__builtin_ia32_tdecrease (void *data, bool is_write)
{
	wqueue_counters_enter_runtime(current_thread);
	tdecrease_n (data, 1, is_write);
}

/* Decrease the synchronization counter by N.  */
void
__builtin_ia32_tdecrease_n (void *data, size_t n, bool is_write)
{
	wqueue_counters_enter_runtime(current_thread);
	tdecrease_n (data, n, is_write);
}

/* Destroy the current thread */
void
__builtin_ia32_tend (void *fp)
{
  wstream_df_frame_p cfp = (wstream_df_frame_p) fp;
  wstream_df_thread_p cthread = current_thread;
  barrier_p cbar = current_barrier;
  barrier_p bar = cfp->own_barrier;

  wqueue_counters_enter_runtime(cthread);

  /* If this task spawned some tasks within a barrier, it needs to
     ensure the barrier gets collected.  */
  if (cbar != NULL)
    {
      if (cbar->barrier_counter_created == 1)
	wstream_df_fatal ("Barrier created without associated tasks.");

      cbar->barrier_unused = true;
      try_pass_barrier (cbar);
      current_barrier = NULL;
    }

  if(slab_allocator_of(fp) == cthread->worker_id)
    inc_wqueue_counter(&cthread->tasks_executed_localalloc, 1);

  __built_in_wstream_df_dec_frame_ref(cfp, 1);

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

void
wstream_df_prematch_dependences (void *v, void *s, bool is_read_view_p)
{
}

void* get_curr_fp(void)
{
  wstream_df_thread_p cthread = current_thread;
  return cthread->current_frame;
}


void __built_in_wstream_df_determine_dominant_prematch_numa_node(void* f)
{
}

void __built_in_wstream_df_determine_dominant_input_numa_node(void* f)
{
}

void __built_in_wstream_df_alloc_view_data_slab(wstream_df_view_p view, size_t size, slab_cache_p slab_cache)
{
	wstream_df_thread_p cthread = current_thread;

	view->data = slab_alloc(cthread, slab_cache, size);

#if ALLOW_WQEVENT_SAMPLING
	wstream_df_frame_p fp = view->owner;
	/* Update data statistics of the frame */
	if(!slab_is_fresh(view->data)) {
		int node_id = slab_numa_node_of(view->data);

		if(node_id >= 0)
			fp->bytes_prematch_nodes[node_id] += size;
	}
#endif
}

void __built_in_wstream_df_alloc_view_data(void* v, size_t size)
{
	wstream_df_view_p view = v;
	slab_cache_p slab_cache = NULL;
	wstream_df_thread_p cthread = current_thread;

	/* Defer allocation for reuse views with reuse predecessors */
	if(is_reuse_view(view)) {
	  view->data = NULL;
	  return;
	}

	slab_cache = cthread->slab_cache;

	__built_in_wstream_df_alloc_view_data_slab(view, size, slab_cache);
}

void __built_in_wstream_df_alloc_view_data_deferred(void* v, size_t size)
{
#if DEFERRED_ALLOC
	wstream_df_view_p view = v;
	view->data = NULL;
#else
	__built_in_wstream_df_alloc_view_data(v, size);
#endif
}

void __built_in_wstream_df_alloc_view_data_vec_deferred(size_t n, void* v, size_t size)
{
  wstream_df_view_p dummy_view = (wstream_df_view_p) v;

  for (size_t i = 0; i < n; ++i)
    {
      wstream_df_view_p view = &((wstream_df_view_p) dummy_view->next)[i];

      if(!dummy_view->reuse_associated_view)
	view->reuse_associated_view = NULL;

      __built_in_wstream_df_alloc_view_data_deferred(view, size);
    }
}

void __built_in_wstream_df_free_view_data(void* v)
{
	wstream_df_view_p view = v;
	wstream_df_thread_p cthread = current_thread;

	assert(view->refcount == 0);

	/* view->data might be NULL due to reuse events */
	if(!view->data)
	  return;

	size_t size = slab_size_of(view->data);
	int node_id;

	if(size < 10000 || (node_id = slab_numa_node_of(view->data)) < 0) {
		slab_free(cthread->slab_cache, view->data);
	} else {
		wstream_df_numa_node_p node = numa_node_by_id(node_id);
		slab_free(&node->slab_cache, view->data);
	}
}

void __built_in_wstream_df_dec_frame_ref(wstream_df_frame_p fp, size_t n)
{
  int sc = __sync_sub_and_fetch (&(fp->refcount), n);

  if(sc == 0)
    wstream_free_frame (fp);
}

void __built_in_wstream_df_inc_frame_ref(wstream_df_frame_p fp, size_t n)
{
  __sync_add_and_fetch (&(fp->refcount), n);
}

#if USE_BROADCAST_TABLES

void dec_broadcast_table_ref(wstream_df_broadcast_table_p bt) {
  wstream_df_thread_p cthread = current_thread;
  int sc = __sync_sub_and_fetch(&bt->refcount, 1);

  if (sc == 0) {
    /* Free local copies */
    for (unsigned i = 0; i < num_numa_nodes; i++) {
      if (bt->node_src[i]) {
        wstream_df_numa_node_p node = numa_node_by_id(i);
        slab_free(&node->slab_cache, (void *)bt->node_src[i]);
      }
    }
    slab_free(cthread->slab_cache, bt);
  }
}

#endif

/* In_view is the input view of the terminating task */
void __built_in_wstream_df_dec_view_ref(wstream_df_view_p in_view, size_t n)
{
  int sc;

  sc = __sync_sub_and_fetch (&in_view->refcount, n);

  if(sc == 0) {
    /* If this is a reusing peek view check broadcast table */
#if USE_BROADCAST_TABLES
    if(in_view->broadcast_table)
      dec_broadcast_table_ref(in_view->broadcast_table);
    else
#endif // USE_BROADCAST_TABLES
	__built_in_wstream_df_free_view_data(in_view);
  }
}

void __built_in_wstream_df_inc_view_ref(wstream_df_view_p view, size_t n)
{
  __sync_add_and_fetch (&(view->refcount), n);
}


void __built_in_wstream_df_dec_view_ref_vec(void* data, size_t num, size_t n)
{
  for (size_t i = 0; i < num; ++i)
    {
      wstream_df_view_p dummy_view = (wstream_df_view_p) data;
      wstream_df_view_p view = &((wstream_df_view_p) dummy_view->next)[i];

      __built_in_wstream_df_dec_view_ref(view, n);
    }
}

void
add_view_to_chain(wstream_df_view_p* start, wstream_df_view_p view)
{
  view->view_chain_next = *start;
  *start = view;
}

void
check_add_view_to_chain(wstream_df_view_p* start, wstream_df_view_p view)
{
  /* Only add view to chain if it's not already included*/
  if(view->view_chain_next) {
    return;
  } else {
    for(wstream_df_view_p it = *start; it; it = it->view_chain_next) {
      if(it == view)
	return;
    }
  }

  add_view_to_chain(start, view);
}

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

  wstream_df_frame_p fp = view->owner;
  int defer_further = 0;

  if(is_read_view_p)
    check_add_view_to_chain(&fp->input_view_chain, view);
  else
    check_add_view_to_chain(&fp->output_view_chain, view);

  trace_state_change(current_thread, WORKER_STATE_RT_RESDEP);

  pthread_mutex_lock (&stream->stream_lock);

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
	      wstream_df_frame_p prod_fp = prod_view->owner;

	      if(is_reuse_view(view)) {
		if(is_reuse_view(prod_view))
		  match_reuse_output_clause_with_reuse_input_clause(prod_view, view);
		else
		  match_reuse_input_clause_with_output_clause(prod_view, view);
	      } else {
		if(is_reuse_view(prod_view))
		  match_reuse_output_clause_with_input_clause(prod_view, view);
	      }

#if DEFERRED_ALLOC
	      /* Data of the consumer view has not been allocated
	       * yet. If we are the only producer, we further defer
	       * allocation until the producer gets ready. Otherwise
	       * we need to allocate here, such that the other
	       * producers get a valid data pointer */
	      if(!view->data) {
		if(!is_reuse_view(view) && view->horizon != prod_view->burst) {
		  __built_in_wstream_df_alloc_view_data(view, view->horizon);
		} else {
		  defer_further = 1;
		  prod_view->data = NULL;
		}
	      }
#endif

	      if(view->horizon == prod_view->burst)
		prod_view->consumer_view = view;

	      if(!defer_further)
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
	      tdecrease_n (prod_fp, 1, 0);
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
	  wstream_df_frame_p prod_fp = view->owner;

	  if(is_reuse_view(cons_view)) {
	    if(is_reuse_view(view))
	      match_reuse_output_clause_with_reuse_input_clause(view, cons_view);
	    else
	      match_reuse_input_clause_with_output_clause(view, cons_view);
	  } else {
	    if(is_reuse_view(view))
	      match_reuse_output_clause_with_input_clause(view, cons_view);
	  }

#if DEFERRED_ALLOC
	  /* Data of the consumer view has not been allocated
	   * yet. If we are the only producer, we further defer
	   * allocation until the producer gets ready. Otherwise
	   * we need to allocate here, such that the other
	   * producers get a valid data pointer */
	  if(!cons_view->data) {
	    if(!is_reuse_view(cons_view) && cons_view->horizon != view->burst) {
	      __built_in_wstream_df_alloc_view_data(cons_view, cons_view->horizon);
	    } else {
	      defer_further = 1;
	      view->data = NULL;
	    }
	  }
#endif

	  if(cons_view->horizon == view->burst)
	    view->consumer_view = cons_view;

	  if(!defer_further)
	    view->data = ((char *)cons_view->data) + cons_view->reached_position;

	  view->owner = cons_view->owner;
	  view->sibling = cons_view->sibling;
	  view->reached_position = cons_view->reached_position;
	  cons_view->reached_position += view->burst;

	  tdecrease_n (prod_fp, 1, 0);

	  if (cons_view->reached_position == cons_view->burst)
	    wstream_df_list_pop (cons_queue);
	}
      else
	wstream_df_list_push (prod_queue, (void *) view);
    }

  pthread_mutex_unlock (&stream->stream_lock);

  trace_state_restore(current_thread);
}

/***************************************************************************/
/* Threads and scheduling.  */
/***************************************************************************/

__attribute__((__optimize__("O1"))) static void worker_thread(void) {
  wstream_df_thread_p cthread = current_thread;

  current_barrier = NULL;

  prng_init(&cthread->rands, cthread->worker_id);
#if CACHE_LAST_STEAL_VICTIM
  cthread->last_steal_from = NULL;
#endif // WQUEUE_PROFILE

  /* Worker 0 has already been initialized */
  if (!cthread->tsc_offset_init) {
    cthread->tsc_offset =
        get_tsc_offset(&global_tsc_ref, cthread->worker_id);
    cthread->tsc_offset_init = 1;
  }

  /* Enable barrier passing if needed.  */
  if (cthread->swap_barrier != NULL) {
    barrier_p bar = cthread->swap_barrier;
    cthread->swap_barrier = NULL;
    try_pass_barrier(bar);
    /* If a swap occurs in try_pass_barrier, then that swap is final
       and this stack is recycled, so no need to restore TLS local
       saves.  */
  }

  trace_state_change(cthread, WORKER_STATE_SEEKING);
  while (true) {
    if (cthread->yield) {
      while (true) {
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 100000000};
        nanosleep(&ts, NULL);
      }
    }

#if ALLOW_PUSHES
#if !ALLOW_PUSH_REORDER
    import_pushes(cthread);
#else
    reorder_pushes(cthread);
#endif
#endif

    wstream_df_frame_p fp = obtain_work(cthread, wstream_df_worker_threads);

    if (fp != NULL) {
      cthread->current_work_fn = fp->work_fn;
      cthread->current_frame = fp;

      wqueue_counters_enter_runtime(current_thread);
      trace_task_exec_start(cthread, fp);
      trace_state_change(cthread, WORKER_STATE_TASKEXEC);

      wqueue_counters_profile_rusage(cthread);
      update_papi(cthread);
      trace_runtime_counters(cthread);

      fp->work_fn(fp);

      wqueue_counters_profile_rusage(cthread);
      trace_runtime_counters(cthread);
      update_papi(cthread);

      __compiler_fence;

      /* It is possible that the work function was suspended then
         its continuation migrated.  We need to restore TLS local
         saves.  */

      /* WARNING: Hack to prevent GCC from deadcoding the next
         assignment (volatile qualifier does not prevent the
         optimization).  CTHREAD is guaranteed not to be null
         here.  */
      if (cthread != NULL) {
#ifdef __aarch64__
        __asm __volatile("str %[current_thread], %[cthread]"
                         : [ cthread ] "=m"(cthread)
                         : [ current_thread ] "r"(current_thread)
                         : "memory");
#else
        __asm__ __volatile__("mov %[current_thread], %[cthread]"
                             : [ cthread ] "=m"(cthread)
                             : [ current_thread ] "R"(current_thread)
                             : "memory");
#endif
      }
      __compiler_fence;

      trace_task_exec_end(cthread, fp);
      cthread->current_work_fn = NULL;
      cthread->current_frame = NULL;

      trace_state_restore(cthread);

      wqueue_counters_enter_runtime(current_thread);
      inc_wqueue_counter(&cthread->tasks_executed, 1);
    } else {
#if WS_NO_YIELD_SPIN
      sched_yield();
#endif
    }
  }
}

void *
wstream_df_worker_thread_fn (void *data)
{
  current_thread = ((wstream_df_thread_p) data);
  wstream_df_thread_p cthread = ((wstream_df_thread_p) data);

  if(cthread->worker_id != 0)
    trace_init(cthread);

  init_wqueue_counters (cthread);

  worker_thread ();
  return NULL;
}

/**
 * Based on parse_affinity of libgomp
 * Taken from gcc/libgomp/env.c with minor changes
 */
static bool
openstream_parse_affinity (unsigned short **cpus_out, size_t *num_cpus_out)
{
  char *env, *end;
  unsigned long cpu_beg, cpu_end, cpu_stride;
  unsigned short *cpus = NULL;
  size_t allocated = 0, used = 0, needed;

  env = getenv ("OPENSTREAM_CPU_AFFINITY");
  if (env == NULL)
    return false;

  do
    {
      while (*env == ' ' || *env == '\t')
	env++;

      cpu_beg = strtoul (env, &end, 0);
      cpu_end = cpu_beg;
      cpu_stride = 1;
      if (env == end || cpu_beg >= 65536)
	goto invalid;

      env = end;
      if (*env == '-')
	{
	  cpu_end = strtoul (++env, &end, 0);
	  if (env == end || cpu_end >= 65536 || cpu_end < cpu_beg)
	    goto invalid;

	  env = end;
	  if (*env == ':')
	    {
	      cpu_stride = strtoul (++env, &end, 0);
	      if (env == end || cpu_stride == 0 || cpu_stride >= 65536)
		goto invalid;

	      env = end;
	    }
	}

      needed = (cpu_end - cpu_beg) / cpu_stride + 1;
      if (used + needed >= allocated)
	{
	  unsigned short *new_cpus;

	  if (allocated < 64)
	    allocated = 64;
	  if (allocated > needed)
	    allocated <<= 1;
	  else
	    allocated += 2 * needed;
	  new_cpus = realloc (cpus, allocated * sizeof (unsigned short));
	  if (new_cpus == NULL)
	    {
	      free (cpus);
	      fprintf (stderr, "not enough memory to store OPENSTREAM_CPU_AFFINITY list");
	      return false;
	    }

	  cpus = new_cpus;
	}

      while (needed--)
	{
	  cpus[used++] = cpu_beg;
	  cpu_beg += cpu_stride;
	}

      while (*env == ' ' || *env == '\t')
	env++;

      if (*env == ',')
	env++;
      else if (*env == '\0')
	break;
    }
  while (1);

  *cpus_out = cpus;
  *num_cpus_out = used;

  return true;

 invalid:
  fprintf (stderr, "Invalid value for enviroment variable OPENSTREAM_CPU_AFFINITY");
  free (cpus);
  return false;
}

int worker_id_to_cpu(unsigned int worker_id)
{
  return wstream_df_worker_threads[worker_id]->cpu->logical_index;
}

static wstream_df_thread_p allocate_worker_struct(hwloc_obj_t on_cpu_mem)
{
	void* ptr;
	size_t size = ROUND_UP(sizeof(wstream_df_thread_t), PAGE_SIZE);

	if(posix_memalign(&ptr, PAGE_SIZE, size))
		wstream_df_fatal("Memory allocation failed!");

  if(bind_memory_to_cpu_memspace(ptr, size, on_cpu_mem)) {
    perror("hwloc_membind");
  }

	return ptr;
}

static void
start_worker (wstream_df_thread_p wstream_df_worker, hwloc_obj_t cpu, size_t num_cpu_affinities,
	      void *(*work_fn) (void *))
{
  pthread_attr_t thread_attr;

  unsigned numa_node = closest_numa_node_of_processing_unit(cpu);
  numa_node_add_thread(numa_node_by_id(numa_node), wstream_df_worker);

#if ALLOW_PUSHES
  fifo_init(&wstream_df_worker->push_fifo);
#endif

  pthread_attr_init (&thread_attr);
  pthread_attr_setdetachstate (&thread_attr, PTHREAD_CREATE_DETACHED);

  cpu_set_t cs = object_glibc_cpuset(wstream_df_worker->cpu);

  int error_val = pthread_attr_setaffinity_np(&thread_attr, sizeof(cs), &cs);
  if (error_val < 0)
    wstream_df_fatal("pthread_attr_setaffinity_np error: %s\n",
                     strerror(error_val));

  void *stack = slab_alloc(NULL, wstream_df_worker_threads[0]->slab_cache, WSTREAM_STACK_SIZE);
  error_val = pthread_attr_setstack (&thread_attr, stack, WSTREAM_STACK_SIZE);
  wstream_df_worker->current_stack = stack;

  error_val = pthread_create(&wstream_df_worker->posix_thread_id, &thread_attr,
                             work_fn, wstream_df_worker);
  if (error_val)
    wstream_df_fatal("pthread_create error: %s\n", strerror(error_val));

  check_bond_to_cpu(wstream_df_worker->posix_thread_id, wstream_df_worker->cpu);

  pthread_attr_destroy (&thread_attr);
}

/* Use main as root task */
int main(void);

__attribute__((constructor))
void pre_main()
{
  unsigned short *cpu_affinities = NULL;
  size_t num_cpu_affinities = 0;
  cpu_set_t cs;

  if (!discover_machine_topology()) {
    wstream_df_fatal("[hwloc] Cannot get architecture information from system");
  }

  /* Restrict topology to the CPU set */
  openstream_parse_affinity(&cpu_affinities, &num_cpu_affinities);
  if (num_cpu_affinities > 0) {
    CPU_ZERO(&cs);
    for (size_t j = 0; j < num_cpu_affinities; ++j)
      CPU_SET(cpu_affinities[j], &cs);
    if (!restrict_topology_to_glibc_cpuset(cs)) {
      wstream_df_error("[hwloc] Warning: could not restrict cpuset");
    }
  }
  free(cpu_affinities);

  /* Restrict the topology according to the number of threads */
  wstream_num_workers = num_available_processing_units();
#ifndef _WSTREAM_DF_NUM_THREADS
  if (getenv("OMP_NUM_THREADS")) {
    unsigned omp_num_threads = atoi(getenv("OMP_NUM_THREADS"));
    if (omp_num_threads > wstream_num_workers) {
      fprintf(stderr,
              "Warning: you are requesting more workers than available "
              "processing units, falling back to %u workers\n",
              wstream_num_workers);
    } else {
      wstream_num_workers = omp_num_threads;
    }
  }
#endif

  init_transfer_matrix();

#if HWLOC_VERBOSE
  fprintf(stdout, "\n[HWLOC Info] Using %u workers distributed as:\n", wstream_num_workers);
#endif
  hwloc_obj_t *processor_mapping = NULL;
  if (!distribute_worker_on_topology(wstream_num_workers, &processor_mapping,
                                     WORKER_DISTRIBUTION_ALGORITHM)) {
    wstream_df_error("[hwloc] Warning: could distribute workers on %d CPUs\n",
                     wstream_num_workers);
  }
#if HWLOC_VERBOSE
  fprintf(stdout, "\n[HWLOC Info] Worker placement topology:\n");
  print_topology_tree(stdout);
#endif

  if (posix_memalign ((void **)&wstream_df_worker_threads, 64,
		      wstream_num_workers * sizeof (wstream_df_thread_t*)))
    wstream_df_fatal ("Out of memory ...");

  /* Add a guard frame for the control program (in case threads catch
     up with the control program).  */

  tsc_reference_offset_init(&global_tsc_ref);

  wstream_df_worker_threads[0] = allocate_worker_struct(processor_mapping[0]);
  current_thread = wstream_df_worker_threads[0];
  current_barrier = NULL;
  // Store the thread structure for easy access to work deque

  numa_nodes_init();

  unsigned numa_node = closest_numa_node_of_processing_unit(processor_mapping[0]);
  numa_node_add_thread(numa_node_by_id(numa_node), current_thread);

#if ALLOW_PUSHES
  fifo_init(&wstream_df_worker_threads[0]->push_fifo);
#endif

  for (unsigned i = 0; i < wstream_num_workers; ++i)
    {
      if(i != 0)
        wstream_df_worker_threads[i] = allocate_worker_struct(processor_mapping[i]);

      cdeque_init (&wstream_df_worker_threads[i]->work_deque, WSTREAM_DF_DEQUE_LOG_SIZE);
      wstream_df_worker_threads[i]->worker_id = i;
      wstream_df_worker_threads[i]->tsc_offset = 0;
      wstream_df_worker_threads[i]->tsc_offset_init = 0;
      wstream_df_worker_threads[i]->swap_barrier = NULL;
      wstream_df_worker_threads[i]->current_work_fn = NULL;
      wstream_df_worker_threads[i]->current_frame = NULL;
      wstream_df_worker_threads[i]->yield = 0;
      wstream_df_worker_threads[i]->cpu = processor_mapping[i];
      wstream_df_worker_threads[i]->cpu->userdata = wstream_df_worker_threads[i];
#if !DISABLE_WQUEUE_LOCAL_CACHE
      wstream_df_worker_threads[i]->own_next_cached_thread = NULL;
#endif // !DISABLE_WQUEUE_LOCAL_CACHE
    }
  
    cs = object_glibc_cpuset(current_thread->cpu);

    int error_val = pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);
    if (error_val < 0)
      wstream_df_fatal("pthread_attr_setaffinity_np error: %s\n",
                       strerror(error_val));
    check_bond_to_cpu(pthread_self(), current_thread->cpu);

    current_thread->tsc_offset =
        get_tsc_offset(&global_tsc_ref, current_thread->worker_id);
    current_thread->tsc_offset_init = 1;

    setup_wqueue_counters();

    for (unsigned i = 1; i < wstream_num_workers; ++i)
      start_worker(wstream_df_worker_threads[i], processor_mapping[i],
                   num_cpu_affinities, wstream_df_worker_thread_fn);

    init_wqueue_counters(wstream_df_worker_threads[0]);

    wstream_df_worker_threads[0]->current_work_fn = (void *)main;
    wstream_df_worker_threads[0]->current_frame = NULL;

  free(processor_mapping);
  trace_init(current_thread);

#if ALLOW_WQEVENT_SAMPLING
  if(signal(SIGUSR1, trace_signal_handler) == SIG_ERR)
    fprintf(stderr, "Cannot install signal handler for SIGUSR1\n");
#endif

  #if TRACE_RT_INIT_STATE
    trace_state_change(current_thread, WORKER_STATE_RT_INIT);
  #endif
}

__attribute__((destructor))
void post_main()
{
  /* Current barrier is the last one, so it allows terminating the
     scheduler functions and exiting once it clears.  */
  wstream_df_taskwait ();

  for (unsigned i = 0; i < wstream_num_workers; ++i)
    wstream_df_worker_threads[i]->yield = 1;

  dump_events_ostv(wstream_num_workers, wstream_df_worker_threads);
  dump_wqueue_counters(wstream_num_workers, wstream_df_worker_threads);
  dump_transfer_matrix(wstream_num_workers);
  openstream_hwloc_cleanup();
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
  pthread_mutex_init (&((wstream_df_stream_p) s)->stream_lock, NULL);
}
/* Allocate and return an array of ARRAY_BYTE_SIZE/ELEMENT_SIZE
   streams.  */
void
wstream_df_stream_ctor (void **s, size_t element_size)
{
  *s = slab_alloc(current_thread, current_thread->slab_cache, sizeof (wstream_df_stream_t));
  init_stream (*s, element_size);
}

void
wstream_df_stream_array_ctor (void **s, size_t num_streams, size_t element_size)
{
  unsigned int i;

  for (i = 0; i < num_streams; ++i)
    {
      s[i] = slab_alloc(current_thread, current_thread->slab_cache, sizeof (wstream_df_stream_t));
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

  wqueue_counters_enter_runtime(current_thread);

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
      slab_free(current_thread->slab_cache, s);
    }
}

void
wstream_df_stream_dtor (void *s)
{
  dec_stream_ref (s);
}

/* Deallocate the array of streams S.  */
void
wstream_df_stream_array_dtor (void **s, size_t num_streams)
{
  unsigned int i;

  for (i = 0; i < num_streams; ++i)
    dec_stream_ref (s[i]);
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

void
wstream_df_prematch_n_dependences (size_t n, void *v, void *s, bool is_read_view_p)
{
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

      /* Only connections with the same burst are allowed for now.  */
      view->burst = dummy_view->burst;
      view->horizon = dummy_view->horizon;

      assert(view->horizon != 0);

      view->owner = dummy_view->owner;

      if(!dummy_view->reuse_associated_view) {
	view->reuse_associated_view = NULL;
      } else {
	view->data = NULL;
      }

      view->reuse_data_view = NULL;
#if USE_BROADCAST_TABLES
      view->broadcast_table = NULL;
#endif
      view->consumer_view = NULL;
      view->refcount = dummy_view->refcount;

      /* FIXME-apop: this is quite tricky, read views may be impacted
	 by old variadic write views' overloaded "reached_position"
	 values.  Fixed for now by clearing the field here.  */
      view->reached_position = 0;

      wstream_df_resolve_dependences ((void *) view, (void *) stream, is_read_view_p);
    }
}

static inline void
broadcast (void *v)
{
  wstream_df_view_p prod_view = (wstream_df_view_p) v;

#if ALLOW_WQEVENT_SAMPLING || USE_BROADCAST_TABLES
  wstream_df_thread_p cthread = current_thread;
#endif

#if ALLOW_WQEVENT_SAMPLING
  trace_state_change(cthread, WORKER_STATE_RT_BCAST);
#endif

  size_t offset = prod_view->reached_position;
  size_t burst = prod_view->burst;
  int use_broadcast_table = 0;

  wstream_df_view_p first_cons_view = prod_view->consumer_view;

  /* Nothing to do if single peek or normal consumer */
  if(!prod_view->sibling) {
    trace_state_restore(cthread);
    return;
  }

#if USE_BROADCAST_TABLES
  wstream_df_broadcast_table_p bt = NULL;
  /* If the producer's burst matches all of the the consumer's
   *  horizons then use a broadcast table */
  if(first_cons_view) {
    use_broadcast_table = 1;

    /* Get NUMA node of source data */
    slab_update_numa_node_of_if_fresh_explicit(first_cons_view->data, cthread->numa_node->id, cthread, 1);

    /* Init broadcast table */
    bt = slab_alloc (cthread, cthread->slab_cache, sizeof (*bt));
    broadcast_table_init(bt);

    first_cons_view->broadcast_table = bt;

    /* Init source entry in broadcast table */
    bt->src_node = slab_numa_node_of(first_cons_view->data);

    bt->refcount = 1;
    bt->node_src[bt->src_node] = first_cons_view->data;
  }
#endif // USE_BROADCAST_TABLES

  for(wstream_df_view_p peek_view = prod_view->sibling;
      peek_view;
      peek_view = peek_view->sibling)
    {
      if(!peek_view->data &&
	 prod_view->burst == peek_view->horizon &&
	 use_broadcast_table)
	{
	  /* Defer copy */
#if USE_BROADCAST_TABLES
	  peek_view->broadcast_table = bt;
	  bt->refcount++;
#endif
      } else {
#if DEFERRED_ALLOC
	  if(!peek_view->data)
	    __built_in_wstream_df_alloc_view_data(peek_view, peek_view->horizon);
#endif
	  memcpy (((char *)peek_view->data) + offset, first_cons_view->data, burst);
	}

      tdecrease_n ((void *) peek_view->owner, burst, 1);
    }

  trace_state_restore(cthread);
}

void
__builtin_ia32_broadcast (void *v)
{
  wqueue_counters_enter_runtime(current_thread);
  broadcast (v);
}

/* Decrease the synchronization counter by N.  */
void
__builtin_ia32_tdecrease_n_vec (size_t num, void *data, size_t n, bool is_write)
{
  unsigned int i;

  wqueue_counters_enter_runtime(current_thread);

  for (i = 0; i < num; ++i)
    {
      wstream_df_view_p dummy_view = (wstream_df_view_p) data;
      wstream_df_view_p view = &((wstream_df_view_p) dummy_view->next)[i];
      wstream_df_frame_p fp = (wstream_df_frame_p) view->owner;

      if (view->sibling)
	      broadcast ((void *) view);
      tdecrease_n ((void *) fp, n, is_write);
    }
}

void
__builtin_ia32_tick (void *s, size_t burst)
{
  wstream_df_stream_p stream = (wstream_df_stream_p) s;
  wstream_df_list_p cons_queue = &stream->consumer_queue;
  wstream_df_view_p cons_view;

#if WQUEUE_PROFILE
  wstream_df_thread_p cthread = current_thread;
  wqueue_counters_enter_runtime(cthread);
#endif // WQUEUE_PROFILE

  if(cons_queue->active_peek_chain == NULL) {
	  wstream_df_frame_p cons_frame;

	  /* Allocate a fake view, with a fake data block, that allows to
	     keep the same dependence resolution algorithm.  */
	  size_t size = sizeof(wstream_df_view_t) + sizeof(wstream_df_frame_t);

	  /* Guard for TDEC: use "work_fn" field as a marker that this
	     frame is not to be considered in optimizations and should be
	     deallocated if it's TDEC'd.  We assume a function pointer
	     cannot be "0x1".  */
	  cons_frame = __builtin_ia32_tcreate (burst * stream->elem_size - 1, size, (void *)1, false);

	  cons_view = (wstream_df_view_p)((char*)cons_frame)+sizeof(wstream_df_frame_t);
	  cons_view->horizon = burst * stream->elem_size;
	  cons_view->burst = cons_view->horizon;
	  cons_view->owner = cons_frame;
	  cons_view->refcount = 1;
	  cons_view->reached_position = 0;

	  __built_in_wstream_df_alloc_view_data_deferred (cons_view, cons_view->horizon);
	  __built_in_wstream_df_determine_dominant_input_numa_node (cons_frame);

  } else {
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

void __built_in_update_numa_node_of_output_view(void* v)
{
  wstream_df_view_p view = v;
  wstream_df_thread_p cthread = current_thread;

  /* FIXME? */
  if(view->data)
	  slab_update_numa_node_of_if_fresh(view->data, cthread, 1);
}

void __built_in_update_numa_node_of_output_view_vec(size_t num, void* v)
{
  wstream_df_view_p view = v;

  for (size_t i = 0; i < num; ++i)
    {
      wstream_df_view_p pview = &((wstream_df_view_p) view->next)[i];
      __built_in_update_numa_node_of_output_view(pview);
    }
}

void __built_in_wstream_df_trace_view_access(void* v, int is_write)
{
#if !ALLOW_WQEVENT_SAMPLING
  wstream_df_fatal ("Event sampling not active, but trace_view_access called!");
  assert(0);
#endif

#if ALLOW_WQEVENT_SAMPLING
  wstream_df_thread_p cthread = current_thread;
  wstream_df_view_p view = v;

  if(is_write) {
    if(is_reuse_view(view))
      trace_data_write(cthread, view->burst, (uint64_t)view->reuse_associated_view->data);
    else
      trace_data_write(cthread, view->burst, (uint64_t)view->data);
  } else {
      int node_id = slab_numa_node_of(view->data);
      wstream_df_thread_p leader = leader_of_numa_node_id(node_id);

      if(!leader)
	trace_data_read(cthread, 0, slab_size_of(view->data), 0, view->data);
      else
	trace_data_read(cthread, leader->cpu->logical_index, slab_size_of(view->data), 0, view->data);
  }
#endif // ALLOW_WQEVENT_SAMPLING
}

void __built_in_wstream_df_trace_view_access_vec(size_t num, void* v, int is_write)
{
#if ALLOW_WQEVENT_SAMPLING
  wstream_df_view_p view = v;

  for (size_t i = 0; i < num; ++i)
    {
      wstream_df_view_p pview = &((wstream_df_view_p) view->next)[i];
      __built_in_wstream_df_trace_view_access(pview, is_write);
    }
#endif // ALLOW_WQEVENT_SAMPLING
}

#if ALLOW_WQEVENT_SAMPLING
static void trace_signal_handler(int sig)
{
  dump_events_ostv(wstream_num_workers, wstream_df_worker_threads);
}
#endif

void openstream_pause_hardware_counters_single(wstream_df_thread_p th)
{
#if WS_PAPI_PROFILE
	/* Event set will continue counting, but no values will be recorded */
	if(th->papi_num_events > 0) {
		th->papi_count = 0;
	}
#endif
}

void openstream_start_hardware_counters(void) {
#if ALLOW_WQEVENT_SAMPLING
  wstream_df_thread_p cthread = current_thread;
  trace_measure_start(cthread);
#endif

#if WS_PAPI_PROFILE
	/* Potentially re-starting after stopping, so reset the values before reading
	*  in order to discard counts from the unmonitored phase
	*/
	if(cthread->papi_num_events > 0){
		for(unsigned i = 0; i < wstream_num_workers; i++){
			wstream_df_worker_threads[i]->papi_reset = 1;
			wstream_df_worker_threads[i]->papi_count = 1;
		}
	}
#endif // WS_PAPI_PROFILE
}

void openstream_pause_hardware_counters(void)
{
	for(unsigned i = 0; i < wstream_num_workers; i++)
	  openstream_pause_hardware_counters_single(wstream_df_worker_threads[i]);

	trace_measure_end(cthread);
}
