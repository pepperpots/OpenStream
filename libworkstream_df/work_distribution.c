#include "work_distribution.h"
#include "arch.h"

#if ALLOW_PUSH_REORDER
/*
 * Compares two frames' transfer costs
 */
static int compare_frame_costs(const void* a, const void* b)
{
  const wstream_df_frame_cost_p fca = (const wstream_df_frame_cost_p)a;
  const wstream_df_frame_cost_p fcb = (const wstream_df_frame_cost_p)b;

  if(fca->cost < fcb->cost)
    return -1;
  else if(fca->cost > fcb->cost)
    return 1;

  return 0;
}

/*
 * Reorders pushed frames and frames from the work-stealing deque
 * with respect to the transfer costs and adds the resulting sequence
 * to the deque.
 */
void reorder_pushes(wstream_df_thread_p cthread)
{
  wstream_df_frame_p import;
  int insert_pos = 0;
  int i;

  memset(cthread->push_reorder_slots, 0, sizeof(cthread->push_reorder_slots));

  /* Put pushed threads into reorder buffer */
  for(i = 0; i < NUM_PUSH_SLOTS; i++) {
    if(!fifo_popfront(&cthread->push_fifo, (void**)&import))
      break;

    cthread->push_reorder_slots[insert_pos].frame = import;
    cthread->push_reorder_slots[insert_pos].cost = mem_cache_misses(cthread) - import->cache_misses[cthread->cpu];
    insert_pos++;

    inc_wqueue_counter(&cthread->steals_pushed, 1);
  }

  /* Put cached thread into reorder buffer */
  if(cthread->own_next_cached_thread && insert_pos < NUM_PUSH_REORDER_SLOTS) {
    import = cthread->own_next_cached_thread;
    cthread->push_reorder_slots[insert_pos].frame = import;
    cthread->push_reorder_slots[insert_pos].cost = mem_cache_misses(cthread) - import->cache_misses[cthread->cpu];
    insert_pos++;
    cthread->own_next_cached_thread = NULL;
  }


  if(insert_pos > 0) {
    trace_state_change(cthread, WORKER_STATE_RT_REORDER);

    /* Fill up with threads from the work-stealing deque */
    while(insert_pos < NUM_PUSH_REORDER_SLOTS) {
      import = cdeque_take(&cthread->work_deque);

      if(import != NULL) {
	cthread->push_reorder_slots[insert_pos].frame = import;
	cthread->push_reorder_slots[insert_pos].cost = mem_cache_misses(cthread) - import->cache_misses[cthread->cpu];
	insert_pos++;
      } else {
	break;
      }
    }

    /* Sort threads by transfer cost */
    qsort(cthread->push_reorder_slots, insert_pos, sizeof(wstream_df_frame_cost_t), compare_frame_costs);

    /* Put frame with lowest cost into the cache */
    cthread->own_next_cached_thread = cthread->push_reorder_slots[0].frame;

    /* Push other frames onto the work-stealing deque */
    for(i = insert_pos-1; i >= 1; i--)
      cdeque_push_bottom (&cthread->work_deque, cthread->push_reorder_slots[i].frame);

    trace_state_restore(cthread);
  }
}
#endif /* ALLOW_PUSH_REORDER */

#if ALLOW_PUSHES
/*
 * Transfers threads from the push buffer to the work-stealing deque
 */
void import_pushes(wstream_df_thread_p cthread)
{
  wstream_df_frame_p import;

  while(fifo_popfront(&cthread->push_fifo, (void**)&import)) {
    inc_wqueue_counter(&cthread->steals_pushed, 1);

    if(cthread->own_next_cached_thread == NULL) {
      cthread->own_next_cached_thread = import;
    } else {
      /* Cache is full, compare number of cache misses since creation / last write */
      if(import->cache_misses[cthread->cpu] > cthread->own_next_cached_thread->cache_misses[cthread->cpu]) {
	cdeque_push_bottom (&cthread->work_deque, cthread->own_next_cached_thread);
	cthread->own_next_cached_thread = import;
      } else {
	cdeque_push_bottom (&cthread->work_deque, import);
      }
    }
  }
}

/* Determines whether a push of fp to another worker is considered beneficial.
 * If a push would be beneficial, the function returns 1 and saves the identifier
 * of the worker suited best for execution in target_worker. Otherwise 0 is
 * returned.
 */
int work_push_beneficial(wstream_df_frame_p fp, wstream_df_thread_p cthread, int num_workers, int* target_worker)
{
  int cpu;
  int worker_id;

  /* By default the current worker is suited best */
  unsigned int max_worker = cthread->worker_id;
  int max_data = fp->bytes_cpu[cthread->cpu];

  /* Overhead for pushing small frames is too high */
  if(fp->size < PUSH_MIN_FRAME_SIZE)
    return 0;

  /* Determine which worker write most of the frame's input data */
  for(worker_id = 0; worker_id < num_workers; worker_id++)
    {
      cpu = worker_id_to_cpu(worker_id);

      if(fp->bytes_cpu[cpu] > max_data) {
	max_data = fp->bytes_cpu[cpu];
	max_worker = worker_id;
      }
    }

  /* Final check */
  if(/* Target worker should have written at least X% more data */
     max_data > PUSH_MIN_REL_FRAME_SIZE * fp->bytes_cpu[cthread->cpu] &&
     /* Only migrate to a different worker */
     max_worker != cthread->worker_id &&
     /* Do not migrate to workers that are too close in the memory hierarchy */
     mem_lowest_common_level(cthread->cpu, worker_id_to_cpu(max_worker)) >= PUSH_MIN_MEM_LEVEL)
    {
      *target_worker = max_worker;
      return 1;
    }

  return 0;

  /* OLD CODE WITH WORKER SELECTION BASED ON TRANSFER COSTS */
  /* unsigned long long xfer_costs[MAX_CPUS]; */
  /* int allocator_id, allocator_cpu; */
  /* unsigned long long min_costs; */
  /* unsigned int min_worker; */

  /* trace_state_change(cthread, WORKER_STATE_RT_ESTIMATE_COSTS); */
  /* long long cache_misses_now[MAX_CPUS]; */
  /* memset(cache_misses_now, 0, sizeof(cache_misses_now)); */

  /* for(worker_id = 0; worker_id < num_workers; worker_id++) { */
  /* 	cpu = worker_id_to_cpu(worker_id); */
  /* 	cache_misses_now[cpu] = mem_cache_misses(&wstream_df_worker_threads[worker_id]); */
  /* } */

  /* allocator_id = wstream_allocator_of(fp); */
  /* allocator_cpu = worker_id_to_cpu(allocator_id); */

  /* mem_estimate_frame_transfer_costs(cthread->cpu, */
  /* 					fp->bytes_cpu, */
  /* 					fp->cache_misses, */
  /* 					cache_misses_now, */
  /* 					allocator_cpu, */
  /* 					xfer_costs); */

  /* trace_state_restore(cthread); */

  /* min_worker = cthread->worker_id; */
  /* min_costs = xfer_costs[cthread->worker_id]; */

  /* ... if(xfer_costs[cpu] < min_costs) { */
  /*   min_costs = xfer_costs[cpu]; */
  /*   min_worker = worker_id; */
  /* } */

}

/*
 * Tries to transfer a frame fp to a worker specified by target_worker by
 * pushing it into its queue of pushed threads. If the push is successful,
 * the function returns 1, otherwise 0.
 */
int work_try_push(wstream_df_frame_p fp,
		  int target_worker,
		  wstream_df_thread_p cthread,
		  wstream_df_thread_p wstream_df_worker_threads)
{
  int level;
  int curr_owner;
  int fp_size;

  /* Save current owner for statistics and update new owner */
  curr_owner = fp->last_owner;
  fp->last_owner = cthread->worker_id;

  /* We need to copy frame attributes used afterwards as the frame will
   * be under control of the target worker once it is pushed.
   */
  fp_size = fp->size;

  if(fifo_pushback(&wstream_df_worker_threads[target_worker].push_fifo, fp)) {
    /* Push was successful, update traces and statistics */
    level = mem_lowest_common_level(cthread->cpu, worker_id_to_cpu(target_worker));
    inc_wqueue_counter(&cthread->pushes_mem[level], 1);

    trace_push(cthread, target_worker, worker_id_to_cpu(target_worker), fp_size);
    return 1;
  }

  /* Push failed, restore owner and update statistics */
  fp->last_owner = curr_owner;
  inc_wqueue_counter(&cthread->pushes_fails, 1);

  return 0;
}

#endif /* ALLOW_PUSHES */

static wstream_df_frame_p work_steal(wstream_df_thread_p cthread, wstream_df_thread_p wstream_df_worker_threads)
{
  int level, attempt, sibling_num;
  unsigned int steal_from = 0;
  unsigned int steal_from_cpu = 0;
  wstream_df_frame_p fp = NULL;

#if CACHE_LAST_STEAL_VICTIM
  /* Try to steal from the last victim worker's deque */
  if(cthread->last_steal_from != -1) {
    fp = cdeque_steal (&wstream_df_worker_threads[cthread->last_steal_from].work_deque);

    if(fp == NULL) {
      inc_wqueue_counter(&cthread->steals_fails, 1);
      cthread->last_steal_from = -1;
    }
  }
#endif

  /* Try to steal from another worker's deque.
   * Start with workers at the lowest common level, i.e. sharing the L2 cache.
   * If the target deque is empty and the maximum number of steal attempts on
   * the level is reached, switch to the next highest level.
   */
  for(level = 0; level < MEM_NUM_LEVELS && fp == NULL; level++)
    {
      for(attempt = 0;
	  attempt < mem_num_steal_attempts_at_level(level) && fp == NULL;
	  attempt++)
	{
	  cthread->rands = cthread->rands * 1103515245 + 12345;
	  sibling_num = (cthread->rands >> 16) % mem_cores_at_level(level);
	  steal_from_cpu = mem_nth_sibling_at_level(level, cthread->cpu, sibling_num);

	  if(cpu_used(steal_from_cpu)) {
	    steal_from = cpu_to_worker_id(steal_from_cpu);
	    fp = cdeque_steal (&wstream_df_worker_threads[steal_from].work_deque);

	    if(fp == NULL) {
	      inc_wqueue_counter(&cthread->steals_fails, 1);

#if CACHE_LAST_STEAL_VICTIM
	      cthread->last_steal_from = -1;
#endif
	    }
	  }
	}
    }

  /* Check if any of the steal attempts succeeded */
  if(fp != NULL)
    {
      inc_wqueue_counter(&cthread->steals_mem[level], 1);
      trace_steal(cthread, steal_from, worker_id_to_cpu(steal_from), fp->size);
      fp->steal_type = STEAL_TYPE_STEAL;

#if CACHE_LAST_STEAL_VICTIM
      cthread->last_steal_from = steal_from;
#endif

      fp->last_owner = steal_from;
    }

  return fp;
}

static wstream_df_frame_p work_cache_take(wstream_df_thread_p cthread)
{
  wstream_df_frame_p fp = NULL;

  /* Try to obtain frame from the local cache */
  fp = cthread->own_next_cached_thread;
  __compiler_fence;

  /* Check if cache was full */
  if (fp != NULL) {
    cthread->own_next_cached_thread = NULL;
    inc_wqueue_counter(&cthread->steals_owncached, 1);
  }

  return fp;
}

static wstream_df_frame_p work_take(wstream_df_thread_p cthread)
{
  wstream_df_frame_p fp = NULL;

  fp = (wstream_df_frame_p)  (cdeque_take (&cthread->work_deque));

  if (fp != NULL)
    inc_wqueue_counter(&cthread->steals_ownqueue, 1);

  return fp;
}

wstream_df_frame_p obtain_work(wstream_df_thread_p cthread,
			       wstream_df_thread_p wstream_df_worker_threads,
			       uint64_t* misses, uint64_t* allocator_misses)
{
  unsigned int cpu;
  int level;
  int allocator, allocator_cpu;
  wstream_df_frame_p fp = NULL;

  /* Try to obtain frame from the local cache */
  fp = work_cache_take(cthread);

  /* Try to obtain frame from the local work deque */
  if (fp == NULL)
    fp = work_take(cthread);

  /* Cache and deque are both empty -> steal */
  if(fp == NULL)
    fp = work_steal(cthread, wstream_df_worker_threads);

  /* A frame pointer could be obtained (locally or via a steal) */
  if (fp != NULL)
    {
      *misses = 0;

      /* Update memory transfer statistics */
      for(cpu = 0; cpu < MAX_CPUS; cpu++)
	{
	  if(fp->bytes_cpu[cpu])
	    {
	      level = mem_lowest_common_level(cthread->cpu, cpu);
	      inc_wqueue_counter(&cthread->bytes_mem[level], fp->bytes_cpu[cpu]);
	      inc_transfer_matrix_entry(cthread->cpu, cpu, fp->bytes_cpu[cpu]);

	      *misses += mem_cache_misses(&wstream_df_worker_threads[cpu_to_worker_id(cpu)]) - fp->cache_misses[cpu];
	    }
	}

      allocator = wstream_allocator_of(fp);
      allocator_cpu = worker_id_to_cpu(allocator);
      *allocator_misses = mem_cache_misses(&wstream_df_worker_threads[allocator]) - fp->cache_misses[allocator_cpu];
    }

  return fp;
}
