#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "trace.h"
#include "wstream_df.h"
#include "profiling.h"
#include "arch.h"
#include "work_distribution.h"

/***************************************************************************/
/***************************************************************************/
/* The current frame pointer, thread data, barrier and saved barrier
   for lastprivate implementation, are stored here in TLS.  */
static __thread wstream_df_thread_p current_thread = NULL;
static __thread barrier_p current_barrier = NULL;

static wstream_df_thread_p wstream_df_worker_threads;
static int num_workers;
static int* wstream_df_worker_cpus;


/*************************************************************************/
/*******             BARRIER/SYNC Handling                         *******/
/*************************************************************************/

static void worker_thread ();

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

  wstream_alloc (&current_thread->slab_cache, &barrier, 64, sizeof (barrier_t));
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
	wstream_free (&current_thread->slab_cache, bar);
      else
	{
	  wstream_df_thread_p cthread = current_thread;
	  trace_task_exec_end(cthread);
	  cthread->current_work_fn = NULL;
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

      wstream_alloc(&cthread->slab_cache, &stack, 64, WSTREAM_STACK_SIZE);
      ws_prepcontext (&ctx, stack, WSTREAM_STACK_SIZE, worker_thread);
      ws_prepcontext (&cbar->continuation_context, NULL, WSTREAM_STACK_SIZE, NULL);

      cthread->swap_barrier = cbar;
      cthread->current_stack = stack;
      cthread->current_work_fn = NULL;

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
      wstream_free (&cthread->slab_cache, cthread->current_stack);
    }

  wstream_free (&cthread->slab_cache, cbar);
  /* Restore thread-local variables.  */
  cthread->current_stack = save_stack;
  current_barrier = save_bar;  /* If this is a LP sync, restore barrier.  */
  cthread->current_work_fn = save_current_work_fn;
}

/***************************************************************************/
/***************************************************************************/

/* Create a new thread, with frame pointer size, and sync counter */
void *
__builtin_ia32_tcreate (size_t sc, size_t size, void *wfn, bool has_lp)
{
  wstream_df_frame_p frame_pointer;
  barrier_p cbar = current_barrier;
  wstream_df_thread_p cthread = current_thread;

  __compiler_fence;

  wqueue_counters_enter_runtime(cthread);
  trace_state_change(cthread, WORKER_STATE_RT_TCREATE);
  trace_event(cthread, WQEVENT_TCREATE);

  wstream_alloc(&cthread->slab_cache, &frame_pointer, 64, size);
  /* memset (frame_pointer, 0, size); */

  frame_pointer->synchronization_counter = sc;
  frame_pointer->size = size;
  frame_pointer->last_owner = cthread->worker_id;
  frame_pointer->steal_type = STEAL_TYPE_UNKNOWN;
  frame_pointer->work_fn = (void (*) (void *)) wfn;
  frame_pointer->creation_timestamp = rdtsc();
  frame_pointer->cache_misses[cthread->worker_id] = mem_cache_misses(cthread);

  inc_wqueue_counter(&cthread->tasks_created, 1);

  memset(frame_pointer->bytes_cpu, 0, sizeof(frame_pointer->bytes_cpu));
  memset(frame_pointer->cache_misses, 0, sizeof(frame_pointer->cache_misses));

  if (has_lp)
    {
      barrier_p temp_bar = cbar;
      cbar = wstream_df_create_barrier ();
      current_barrier = cbar;
      cbar->save_barrier = temp_bar;
    }

  if (cbar == NULL)
    {
      cbar = wstream_df_create_barrier ();
      current_barrier = cbar;
    }

  cbar->barrier_counter_created++;
  frame_pointer->own_barrier = cbar;

  trace_state_restore(cthread);
  return frame_pointer;
}

/* Decrease the synchronization counter by N.  */
static inline void
tdecrease_n (void *data, size_t n, bool is_write)
{
  wstream_df_frame_p fp = (wstream_df_frame_p) data;
  wstream_df_thread_p cthread = current_thread;

  trace_state_change(cthread, WORKER_STATE_RT_TDEC);

  if(is_write) {
    inc_wqueue_counter(&fp->bytes_cpu[cthread->cpu], n);

    if(fp->cache_misses[cthread->cpu] == 0)
      fp->cache_misses[cthread->cpu] = mem_cache_misses(cthread);
  }

  int sc = 0;

  if (fp->synchronization_counter != (int) n)
    sc = __sync_sub_and_fetch (&(fp->synchronization_counter), n);
  /* else the atomic sub would return 0.  This relies on the fact that
     the synchronization_counter is strictly decreasing.  */

  /* Schedule the thread if its synchronization counter reaches 0.  */
  if (sc == 0)
    {
      fp->last_owner = cthread->worker_id;
      fp->steal_type = STEAL_TYPE_PUSH;
      fp->ready_timestamp = rdtsc();

      if (fp->work_fn == (void *) 1)
	{
	  wstream_free(&cthread->slab_cache, fp);
	  trace_state_restore(cthread);
	  return;
	}

#if ALLOW_PUSHES
      int target_worker;

      /* Check whether the frame should be pushed somewhere else */
      if(work_push_beneficial(fp, cthread, num_workers, &target_worker))
	{
	  if(work_try_push(fp, target_worker, cthread, wstream_df_worker_threads))
	    {
	      trace_state_restore(cthread);
	      return;
	    }
	}
#endif

      if (cthread->own_next_cached_thread != NULL)
	cdeque_push_bottom (&cthread->work_deque,
			    (wstream_df_type) cthread->own_next_cached_thread);
      cthread->own_next_cached_thread = fp;
    }

  trace_state_restore(cthread);
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

  if(wstream_allocator_of(fp) == cthread->worker_id)
    inc_wqueue_counter(&cthread->tasks_executed_localalloc, 1);

  wstream_free (&cthread->slab_cache, cfp);

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
	  void *prod_fp = view->owner;
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

__attribute__((__optimize__("O1")))
static void
worker_thread (void)
{
  wstream_df_thread_p cthread = current_thread;

  current_barrier = NULL;

  cthread->rands = 77777 + cthread->worker_id * 19;
  cthread->last_steal_from = -1;

  /* Enable barrier passing if needed.  */
  if (cthread->swap_barrier != NULL)
    {
      barrier_p bar = cthread->swap_barrier;
      cthread->swap_barrier = NULL;
      try_pass_barrier (bar);
      /* If a swap occurs in try_pass_barrier, then that swap is final
	 and this stack is recycled, so no need to restore TLS local
	 saves.  */
    }

  while (true)
    {
#if ALLOW_PUSHES
#if !ALLOW_PUSH_REORDER
      import_pushes(cthread);
#else
      reorder_pushes(cthread);
#endif
#endif

      uint64_t misses, allocator_misses;
      wstream_df_frame_p fp = obtain_work(cthread, wstream_df_worker_threads, &misses, &allocator_misses);

      if(fp != NULL)
	{
	  cthread->current_work_fn = fp->work_fn;

	  wqueue_counters_enter_runtime(current_thread);
	  trace_task_exec_start(cthread, fp->last_owner, fp->steal_type, fp->creation_timestamp, fp->ready_timestamp, fp->size, misses, allocator_misses);
	  trace_state_change(cthread, WORKER_STATE_TASKEXEC);
	  fp->work_fn (fp);

	  __compiler_fence;

	  /* It is possible that the work function was suspended then
	     its continuation migrated.  We need to restore TLS local
	     saves.  */

	  /* WARNING: Hack to prevent GCC from deadcoding the next
	     assignment (volatile qualifier does not prevent the
	     optimization).  CTHREAD is guaranteed not to be null
	     here.  */
	  if (cthread != NULL)
	    __asm__ __volatile__ ("mov %[current_thread], %[cthread]"
				  : [cthread] "=m" (cthread) : [current_thread] "R" (current_thread) : "memory");

	  __compiler_fence;

	  trace_task_exec_end(cthread);

	  cthread->current_work_fn = NULL;
	  trace_state_change(cthread, WORKER_STATE_SEEKING);

	  update_papi(cthread);

	  wqueue_counters_enter_runtime(current_thread);
	  inc_wqueue_counter(&cthread->tasks_executed, 1);
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
  current_thread = ((wstream_df_thread_p) data);
  wstream_df_thread_p cthread = ((wstream_df_thread_p) data);

  if (cthread->worker_id != 0)
    {
	    wstream_init_alloc(&cthread->slab_cache, cthread->worker_id);
    }

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
  return wstream_df_worker_threads[worker_id].cpu;
}

int cpu_to_worker_id(int cpu)
{
  return wstream_df_worker_cpus[cpu];
}

int cpu_used(int cpu)
{
  return (cpu_to_worker_id(cpu) != -1);
}

static void
start_worker (wstream_df_thread_p wstream_df_worker, int ncores,
	      unsigned short *cpu_affinities, size_t num_cpu_affinities,
	      void *(*work_fn) (void *))
{
  pthread_attr_t thread_attr;
  unsigned int i;

  int id = wstream_df_worker->worker_id;

  if (cpu_affinities == NULL)
    wstream_df_worker->cpu = id % ncores;
  else
    wstream_df_worker->cpu = cpu_affinities[id % num_cpu_affinities];

#if ALLOW_PUSHES
  fifo_init(&wstream_df_worker->push_fifo);
#endif

#ifdef TRACE_RT_INIT_STATE
  if(wstream_df_worker->worker_id != 0)
    trace_init(wstream_df_worker);
#else
  trace_init(wstream_df_worker);
#endif

#ifdef _PRINT_STATS
  printf ("worker %d mapped to core %d\n", id, wstream_df_worker->cpu);
#endif

  pthread_attr_init (&thread_attr);
  pthread_attr_setdetachstate (&thread_attr, PTHREAD_CREATE_DETACHED);

  cpu_set_t cs;
  CPU_ZERO (&cs);
  CPU_SET (wstream_df_worker->cpu, &cs);

  assert(!cpu_used(wstream_df_worker->cpu));
  wstream_df_worker_cpus[wstream_df_worker->cpu] = wstream_df_worker->worker_id;

  int errno = pthread_attr_setaffinity_np (&thread_attr, sizeof (cs), &cs);
  if (errno < 0)
    wstream_df_fatal ("pthread_attr_setaffinity_np error: %s\n", strerror (errno));

  void *stack;
  wstream_alloc(&wstream_df_worker_threads[0].slab_cache, &stack, 64, WSTREAM_STACK_SIZE);
  errno = pthread_attr_setstack (&thread_attr, stack, WSTREAM_STACK_SIZE);
  wstream_df_worker->current_stack = stack;

  pthread_create (&wstream_df_worker->posix_thread_id, &thread_attr,
		  work_fn, wstream_df_worker);

  CPU_ZERO (&cs);
  errno = pthread_getaffinity_np (wstream_df_worker->posix_thread_id, sizeof (cs), &cs);
  if (errno != 0)
    wstream_df_fatal ("pthread_getaffinity_np error: %s\n", strerror (errno));

  for (i = 0; i < CPU_SETSIZE; i++)
    if (CPU_ISSET (i, &cs) && i != wstream_df_worker->cpu)
      wstream_df_error ("got affinity to core %d, expecting %d\n", i, wstream_df_worker->cpu);

  if (!CPU_ISSET (wstream_df_worker->cpu, &cs))
    wstream_df_error ("no affinity to core %d\n", wstream_df_worker->cpu);

  pthread_attr_destroy (&thread_attr);
}

/* Use main as root task */
void main(void);

__attribute__((constructor))
void pre_main()
{
  int i, ncores;
  unsigned short *cpu_affinities = NULL;
  size_t num_cpu_affinities = 0;

  init_transfer_matrix();

  /* Set workers number as the number of cores.  */
#ifndef _WSTREAM_DF_NUM_THREADS
  if(getenv("OMP_NUM_THREADS"))
    num_workers = atoi(getenv("OMP_NUM_THREADS"));
  else
    num_workers = wstream_df_num_cores ();
#else
  num_workers = _WSTREAM_DF_NUM_THREADS;
#endif

  if (posix_memalign ((void **)&wstream_df_worker_threads, 64,
		      num_workers * sizeof (wstream_df_thread_t)))
    wstream_df_fatal ("Out of memory ...");

  if (posix_memalign ((void **)&wstream_df_worker_cpus, 64,
		      MAX_CPUS * sizeof (unsigned int)))
    wstream_df_fatal ("Out of memory ...");

  for(i = 0; i < MAX_CPUS; i++)
    wstream_df_worker_cpus[i] = -1;

  ncores = wstream_df_num_cores ();

  wstream_init_alloc(&wstream_df_worker_threads[0].slab_cache, 0);

#ifdef _PRINT_STATS
  printf ("Creating %d workers for %d cores\n", num_workers, ncores);
#endif

  for (i = 0; i < num_workers; ++i)
    {
      cdeque_init (&wstream_df_worker_threads[i].work_deque, WSTREAM_DF_DEQUE_LOG_SIZE);
      wstream_df_worker_threads[i].worker_id = i;
      wstream_df_worker_threads[i].own_next_cached_thread = NULL;
      wstream_df_worker_threads[i].swap_barrier = NULL;
      wstream_df_worker_threads[i].current_work_fn = NULL;
    }

  /* Add a guard frame for the control program (in case threads catch
     up with the control program).  */
  current_thread = &wstream_df_worker_threads[0];
  current_barrier = NULL;

#ifdef TRACE_RT_INIT_STATE
  trace_init(&wstream_df_worker_threads[0]);
  trace_state_change(current_thread, WORKER_STATE_RT_INIT);
#endif

  setup_wqueue_counters();

  openstream_parse_affinity(&cpu_affinities, &num_cpu_affinities);
  for (i = 1; i < num_workers; ++i)
    start_worker (&wstream_df_worker_threads[i], ncores, cpu_affinities, num_cpu_affinities,
		  wstream_df_worker_thread_fn);

  if (cpu_affinities == NULL)
    current_thread->cpu = 0;
  else
    current_thread->cpu = cpu_affinities[0];
  wstream_df_worker_cpus[current_thread->cpu] = 0;

  free (cpu_affinities);
  init_wqueue_counters(&wstream_df_worker_threads[0]);

  wstream_df_worker_threads[0].current_work_fn = (void*)main;
}

__attribute__((destructor))
void post_main()
{
  /* Current barrier is the last one, so it allows terminating the
     scheduler functions and exiting once it clears.  */
  wstream_df_taskwait ();

  dump_events_ostv(num_workers, wstream_df_worker_threads);
  dump_avg_state_parallelism(WORKER_STATE_TASKEXEC, 1000, num_workers, wstream_df_worker_threads);
  dump_average_task_duration_summary(num_workers, wstream_df_worker_threads);
  dump_average_task_duration(1000, num_workers, wstream_df_worker_threads);
  dump_task_duration_histogram(num_workers, wstream_df_worker_threads);
  dump_wqueue_counters(num_workers, wstream_df_worker_threads);
  dump_transfer_matrix(num_workers);
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
  wstream_alloc(&current_thread->slab_cache, s, 64, sizeof (wstream_df_stream_t));
  init_stream (*s, element_size);
}

void
wstream_df_stream_array_ctor (void **s, size_t num_streams, size_t element_size)
{
  unsigned int i;

  for (i = 0; i < num_streams; ++i)
    {
      wstream_alloc(&current_thread->slab_cache, &s[i], 64, sizeof (wstream_df_stream_t));
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
      wstream_free(&current_thread->slab_cache, s);
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
      /* FIXME-apop: this is quite tricky, read views may be impacted
	 by old variadic write views' overloaded "reached_position"
	 values.  Fixed for now by clearing the frames on
	 allocation.  */
      //view->reached_position = 0;

      wstream_df_resolve_dependences ((void *) view, (void *) stream, is_read_view_p);
    }
}

static inline void
broadcast (void *v)
{
  wstream_df_view_p prod_view = (wstream_df_view_p) v;
  wstream_df_view_p cons_view = prod_view;
  wstream_df_thread_p cthread = current_thread;

  trace_state_change(cthread, WORKER_STATE_RT_BCAST);

  size_t offset = prod_view->reached_position;
  size_t burst = prod_view->burst;
  void *base_addr = prod_view->data;

  while ((cons_view = cons_view->sibling) != NULL)
    {
      memcpy (((char *)cons_view->data) + offset, base_addr, burst);
      tdecrease_n ((void *) cons_view->owner, burst, 1);
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

  wqueue_counters_enter_runtime(current_thread);

  /* ASSERT that cons_view->active_peek_chain != NULL !! Otherwise
     this requires passing the matching producers a fake consumer view
     where to write at least during the producers' execution ... */
  if (cons_queue->active_peek_chain == NULL)
    {
      wstream_df_frame_p cons_frame;

      /* Normally an error, but for OMPSs expansion, we need to allow
	 it.  */
      /*  wstream_df_fatal ("TICK (%d) matches no PEEK view, which is unsupported at this time.",
	  burst * stream->elem_size);
      */
      /* Allocate a fake view, with a fake data block, that allows to
	 keep the same dependence resolution algorithm.  */
      size_t size = sizeof (wstream_df_view_t) + burst * stream->elem_size + sizeof (wstream_df_frame_t);
      wstream_alloc(&current_thread->slab_cache, &cons_frame, 64, size);
      memset (cons_frame, 0, size);

      /* Avoid one atomic operation by setting the "next" field (which
	 is the SC in the frame) to the size, which means TDEC won't
	 do anything.  */
      cons_frame->synchronization_counter = burst * stream->elem_size;
      cons_frame->size = size;

      /* Guard for TDEC: use "work_fn" field as a marker that this
	 frame is not to be considered in optimizations and should be
	 deallocated if it's TDEC'd.  We assume a function pointer
	 cannot be "0x1".  */
      cons_frame->work_fn = (void *)1;

      cons_view = (wstream_df_view_p) (((char *) cons_frame) + sizeof (wstream_df_frame_t));
      cons_view->horizon = burst * stream->elem_size;
      cons_view->burst = cons_view->horizon;
      cons_view->owner = cons_frame;
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
