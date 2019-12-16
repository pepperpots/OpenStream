#include "work_distribution.h"
#include "arch.h"
#include "hwloc.h"
#include "numa.h"
#include "prng.h"
#include "reuse.h"
#include "wstream_df.h"

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
    cthread->push_reorder_slots[insert_pos].cost = mem_cache_misses(cthread) - import->cache_misses[cthread->worker_id];
    insert_pos++;

    inc_wqueue_counter(&cthread->steals_pushed, 1);
  }

#if !DISABLE_WQUEUE_LOCAL_CACHE
  /* Put cached thread into reorder buffer */
  if(cthread->own_next_cached_thread && insert_pos < NUM_PUSH_REORDER_SLOTS) {
    import = cthread->own_next_cached_thread;
    cthread->push_reorder_slots[insert_pos].frame = import;
    cthread->push_reorder_slots[insert_pos].cost = mem_cache_misses(cthread) - import->cache_misses[cthread->worker_id];
    insert_pos++;
    cthread->own_next_cached_thread = NULL;
  }
#endif // !DISABLE_WQUEUE_LOCAL_CACHE

  if(insert_pos > 0) {
    trace_state_change(cthread, WORKER_STATE_RT_REORDER);

    /* Fill up with threads from the work-stealing deque */
    while(insert_pos < NUM_PUSH_REORDER_SLOTS) {
      import = cdeque_take(&cthread->work_deque);

      if(import != NULL) {
	cthread->push_reorder_slots[insert_pos].frame = import;
	cthread->push_reorder_slots[insert_pos].cost = mem_cache_misses(cthread) - import->cache_misses[cthread->worker_id];
	insert_pos++;
      } else {
	break;
      }
    }

    /* Sort threads by transfer cost */
    qsort(cthread->push_reorder_slots, insert_pos, sizeof(wstream_df_frame_cost_t), compare_frame_costs);

#if !DISABLE_WQUEUE_LOCAL_CACHE
    /* Put frame with lowest cost into the cache */
    cthread->own_next_cached_thread = cthread->push_reorder_slots[0].frame;

    /* Push other frames onto the work-stealing deque */
    for(i = insert_pos-1; i >= 1; i--)
#else // DISABLE_WQUEUE_LOCAL_CACHE
    for(i = insert_pos-1; i >= 0; i--)
#endif // !DISABLE_WQUEUE_LOCAL_CACHE
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

#if !DISABLE_WQUEUE_LOCAL_CACHE
    if(cthread->own_next_cached_thread == NULL) {
      cthread->own_next_cached_thread = import;
    } else {
      cdeque_push_bottom (&cthread->work_deque, cthread->own_next_cached_thread);
      cthread->own_next_cached_thread = import;
    }
#else // DISABLE_WQUEUE_LOCAL_CACHE
    cdeque_push_bottom(&cthread->work_deque, import);
#endif // !DISABLE_WQUEUE_LOCAL_CACHE
  }
}

int work_push_beneficial_max_writer(wstream_df_frame_p fp, wstream_df_thread_p cthread, int num_workers, int* target_worker)
{
  unsigned int max_worker;
  int max_data;

  /* Determine which worker write most of the frame's input data */
  get_max_worker(fp->bytes_cpu_in, num_workers, &max_worker, &max_data);

  /* By default the current worker is suited best */
  if(fp->bytes_cpu_in[cthread->worker_id] >= max_data) {
      max_worker = cthread->worker_id;
      max_data = fp->bytes_cpu_in[cthread->worker_id];
  }

  if(max_data < PUSH_MIN_REL_FRAME_SIZE * fp->bytes_cpu_in[cthread->worker_id])
    return 0;

  *target_worker = max_worker;

  return 1;
}

int work_push_beneficial_owner(wstream_df_frame_p fp, wstream_df_thread_p cthread, int num_workers, int* target_worker)
{
  unsigned int max_worker;
  int numa_node_id;
  int max_data;
  wstream_df_numa_node_p numa_node;
  unsigned int rand_idx;

  /* Determine node id of owning NUMA node */
  numa_node_id = slab_numa_node_of(fp);

  if(numa_node_id != -1 && cthread->numa_node->id != numa_node_id) {
    /* Choose random worker sharing the target node */
    numa_node = numa_node_by_id(numa_node_id);
    rand_idx = prng_nextn(&cthread->rands, numa_node->num_workers);
    max_worker = numa_node->workers[rand_idx]->worker_id;

    /* Set amount of data to frame size */
    max_data = fp->size;
  } else {
    /* Node unknown, use local worker by default */
      get_max_worker(fp->bytes_cpu_in, num_workers, &max_worker, &max_data);

      /* By default the current worker is suited best */
      if(fp->bytes_cpu_in[cthread->worker_id] >= max_data) {
	max_worker = cthread->worker_id;
	max_data = fp->bytes_cpu_in[cthread->worker_id];
      }

      if(max_data < PUSH_MIN_REL_FRAME_SIZE * fp->bytes_cpu_in[cthread->worker_id])
	return 0;

      max_worker = cthread->worker_id;
      max_data = fp->bytes_cpu_in[cthread->worker_id];
  }

  *target_worker = max_worker;
  return 1;
}

int work_push_beneficial_split_owner(wstream_df_frame_p fp, wstream_df_thread_p cthread, int num_workers, int* target_worker)
{
  unsigned int max_worker;
  int numa_node_id;
  int max_data;
  wstream_df_numa_node_p numa_node;
  unsigned int rand_idx;

  /* Determine node id of owning NUMA node */
  max_data = fp->dominant_input_data_size;
  numa_node_id = fp->dominant_input_data_node_id;

  if(max_data > PUSH_MIN_REL_FRAME_SIZE && numa_node_id != -1 && cthread->numa_node->id != numa_node_id) {
    /* Choose random worker sharing the target node */
    numa_node = numa_node_by_id(numa_node_id);
    rand_idx = prng_nextn(&cthread->rands, numa_node->num_workers);
    max_worker = numa_node->workers[rand_idx]->worker_id;
  } else {
    /* Node unknown, use local worker by default */
      get_max_worker(fp->bytes_cpu_in, num_workers, &max_worker, &max_data);

      /* By default the current worker is suited best */
      if(fp->bytes_cpu_in[cthread->worker_id] >= max_data) {
	max_worker = cthread->worker_id;
	max_data = fp->bytes_cpu_in[cthread->worker_id];
      }

      if(max_data < PUSH_MIN_REL_FRAME_SIZE * fp->bytes_cpu_in[cthread->worker_id])
	return 0;

      max_worker = cthread->worker_id;
      max_data = fp->bytes_cpu_in[cthread->worker_id];
  }

  *target_worker = max_worker;
  return 1;
}

int work_push_beneficial_split_owner_chain(wstream_df_frame_p fp, wstream_df_thread_p cthread, int num_workers, int* target_worker)
{
  unsigned int max_worker;
  int numa_node_id;
  int max_data;
  size_t data[MAX_NUMA_NODES];
  wstream_df_numa_node_p numa_node;
  unsigned int rand_idx;

  /* Overhead for pushing small frames is too high */
  if(fp->dominant_input_data_size < PUSH_MIN_FRAME_SIZE)
    return 0;

  memset(data, 0, sizeof(data));

  for(wstream_df_view_p vi = fp->input_view_chain; vi; vi = vi->view_chain_next) {
    int node_id = slab_numa_node_of(vi->data);
    int factor = 1;

    if(vi->reuse_data_view)
      factor = 1;

    if(node_id != -1)
      data[node_id] += vi->horizon*factor;
  }

  max_data = data[cthread->numa_node->id];
  numa_node_id = cthread->numa_node->id;

  for(int i = 0; i < MAX_NUMA_NODES; i++) {
    if((int)data[i] > max_data) {
      max_data = data[i];
      numa_node_id = i;
    }
  }

  if(max_data > PUSH_MIN_REL_FRAME_SIZE && numa_node_id != -1 && cthread->numa_node->id != numa_node_id) {
    /* Choose random worker sharing the target node */
    numa_node = numa_node_by_id(numa_node_id);
    rand_idx = prng_nextn(&cthread->rands, numa_node->num_workers);;
    max_worker = numa_node->workers[rand_idx]->worker_id;
  } else {
	return 0;
  }

  *target_worker = max_worker;
  return 1;
}

int work_push_beneficial_split_owner_chain_inner_mw(wstream_df_frame_p fp, wstream_df_thread_p cthread, int num_workers, unsigned int* target_worker)
{
  unsigned int max_worker;
  int numa_node_id;
  int max_data;
  size_t data[MAX_NUMA_NODES];
  wstream_df_numa_node_p numa_node;
  unsigned int rand_idx;
  int node_id;

#if defined(PUSH_EQUAL_RANDOM)
    size_t others[MAX_NUMA_NODES];
    int num_others = 0;
#endif

  /* Overhead for pushing small frames is too high */
  if(fp->dominant_input_data_size < PUSH_MIN_FRAME_SIZE)
    return 0;

  memset(data, 0, sizeof(data));

  for(wstream_df_view_p vi = fp->input_view_chain; vi; vi = vi->view_chain_next) {
    /* By default assume that data is going to be reused */
    if(is_reuse_view(vi) && !reuse_view_has_own_data(vi))
      node_id = slab_numa_node_of(vi->reuse_data_view->data);
    else
      node_id = slab_numa_node_of(vi->data);

    int factor = 1;

    if(vi->reuse_data_view)
      factor = 2;

    if(node_id != -1)
      data[node_id] += vi->horizon*factor;
  }

  max_data = data[cthread->numa_node->id];
  numa_node_id = cthread->numa_node->id;

#if defined(PUSH_EQUAL_SEQ)
  for(int i = 0; i < MAX_NUMA_NODES; i++) {
    if((int)data[i] > max_data) {
      max_data = data[i];
      numa_node_id = i;
    }
  }
#elif defined(PUSH_EQUAL_RANDOM)
  for(int i = 0; i < MAX_NUMA_NODES; i++) {
    if((int)data[i] > max_data)
      others[num_others++] = i;

    if((int)data[i] > max_data) {
      max_data = data[i];
      numa_node_id = i;
      num_others = 0;
    }
  }

  if(numa_node_id != cthread->numa_node->id && num_others)
    {
      others[num_others++] = numa_node_id;
      rand_idx = prng_nextn(&cthread->rands, num_others);
      numa_node_id = others[rand_idx];
    }
#else
  #ifdef ALLOW_PUSHES
    #error "No strategy defined for nodes with the same push score!"
  #endif
#endif

  if(max_data > PUSH_MIN_REL_FRAME_SIZE && numa_node_id != -1 && cthread->numa_node->id != numa_node_id) {
    /* Choose random worker sharing the target node */
    numa_node = numa_node_by_id(numa_node_id);
    rand_idx = prng_nextn(&cthread->rands, numa_node->num_workers);
    max_worker = numa_node->workers[rand_idx]->worker_id;
  } else if(cthread->numa_node->id == numa_node_id) {
    /* Node unknown, use local worker by default */
      get_max_worker_same_node(fp->bytes_cpu_in, num_workers, &max_worker, &max_data, numa_node_id);

      /* By default the current worker is suited best */
      if(fp->bytes_cpu_in[cthread->worker_id] >= max_data) {
	max_worker = cthread->worker_id;
	max_data = fp->bytes_cpu_in[cthread->worker_id];
      }

      if(max_data < PUSH_MIN_REL_FRAME_SIZE * fp->bytes_cpu_in[cthread->worker_id])
	return 0;

      max_worker = cthread->worker_id;
      max_data = fp->bytes_cpu_in[cthread->worker_id];
  }

  *target_worker = max_worker;
  return 1;
}

int work_push_beneficial_split_score_nodes(wstream_df_frame_p fp, wstream_df_thread_p cthread, int num_workers, unsigned int* target_worker)
{
  unsigned int max_worker;
  int numa_node_id;
  int max_data;
  size_t data[MAX_NUMA_NODES];
  size_t scores[MAX_NUMA_NODES];
  size_t min_score;
  wstream_df_numa_node_p numa_node;
  int factor;
  unsigned int rand_idx;
  int node_id;
  int input_size = 0;

#if defined(PUSH_EQUAL_RANDOM)
    size_t others[MAX_NUMA_NODES];
    int num_others = 0;
#endif

  memset(data, 0, sizeof(data));
  memset(scores, 0, sizeof(data));

  for(wstream_df_view_p vi = fp->input_view_chain; vi; vi = vi->view_chain_next) {
    /* By default assume that data is going to be reused */
    if(vi->reuse_data_view)
      node_id = slab_numa_node_of(vi->reuse_data_view->data);
    else if(vi->broadcast_table) /* Peek view with deferred copy */
      node_id = -1;
    else
      node_id = slab_numa_node_of(vi->data);

    if(vi->reuse_data_view)
      factor = 2;

    if(node_id != -1)
      data[node_id] += vi->horizon * factor;

    input_size += vi->horizon;
  }

  if(input_size < PUSH_MIN_FRAME_SIZE)
    return 0;

  for(int target_node = 0; target_node < MAX_NUMA_NODES; target_node++)
    for(int source_node = 0; source_node < MAX_NUMA_NODES; source_node++)
      scores[target_node] += data[source_node] * mem_transfer_costs(target_node, source_node);

  min_score = scores[cthread->numa_node->id];
  numa_node_id = cthread->numa_node->id;

#if defined(PUSH_EQUAL_SEQ)
  for(int i = 0; i < MAX_NUMA_NODES; i++) {
    if(scores[i] < min_score) {
      min_score = scores[i];
      numa_node_id = i;
    }
  }
#elif defined(PUSH_EQUAL_RANDOM)
  for(int i = 0; i < MAX_NUMA_NODES; i++) {
    if(scores[i] == min_score)
      others[num_others++] = i;

    if(scores[i] < min_score) {
      min_score = scores[i];
      numa_node_id = i;
      num_others = 0;
    }
  }

  if(numa_node_id != cthread->numa_node->id && num_others)
    {
      others[num_others++] = numa_node_id;

      rand_idx = prng_nextn(&cthread->rands, num_others);
      numa_node_id = others[rand_idx];
    }
#else
  #ifdef ALLOW_PUSHES
    #error "No strategy defined for nodes with the same push score!"
  #endif
#endif


  if(numa_node_id != -1 && cthread->numa_node->id != numa_node_id) {
    /* Choose random worker sharing the target node */
    numa_node = numa_node_by_id(numa_node_id);
    rand_idx = prng_nextn(&cthread->rands, numa_node->num_workers);
    max_worker = numa_node->workers[rand_idx]->worker_id;
  } else {
    return 0;
  }

  *target_worker = max_worker;

  return 1;
}

/* Determines whether a push of fp to another worker is considered beneficial.
 * If a push would be beneficial, the function returns 1 and saves the identifier
 * of the worker suited best for execution in target_worker. Otherwise 0 is
 * returned.
 */
int work_push_beneficial(wstream_df_frame_p fp, wstream_df_thread_p cthread, int num_workers, int* target_worker)
{
  int res;
  unsigned int lcl_target_worker;

#if defined(PUSH_STRATEGY_MAX_WRITER)
  res = work_push_beneficial_max_writer(fp, cthread, num_workers, &lcl_target_worker);
#elif defined(PUSH_STRATEGY_OWNER)
  res = work_push_beneficial_owner(fp, cthread, num_workers, &lcl_target_worker);
#elif defined(PUSH_STRATEGY_SPLIT_OWNER)
  res = work_push_beneficial_split_owner(fp, cthread, num_workers, &lcl_target_worker);
#elif defined(PUSH_STRATEGY_SPLIT_OWNER_CHAIN)
  res = work_push_beneficial_split_owner_chain(fp, cthread, num_workers, &lcl_target_worker);
#elif defined(PUSH_STRATEGY_SPLIT_OWNER_CHAIN_INNER_MW)
  res = work_push_beneficial_split_owner_chain_inner_mw(fp, cthread, num_workers, &lcl_target_worker);
#elif defined(PUSH_STRATEGY_SPLIT_SCORE_NODES)
  res = work_push_beneficial_split_score_nodes(fp, cthread, num_workers, &lcl_target_worker);
/* #elif defined(PUSH_STRATEGY_REUSE_OWNER) */
/*   res = work_push_beneficial_reuse_owner(fp, cthread, num_workers, &lcl_target_worker); */
#else
  #error "No push strategy defined" */
#endif

  if(!res)
    return 0;

  /* Final check */
  if(/* Only migrate to a different worker */
     lcl_target_worker != cthread->worker_id &&
     /* Do not migrate to workers that are too close in the memory hierarchy */
     mem_lowest_common_level(cthread->worker_id, worker_id_to_cpu(lcl_target_worker)) >= PUSH_MIN_MEM_LEVEL)
    {
      *target_worker = lcl_target_worker;
      return 1;
    }

  return 0;
}

/*
 * Tries to transfer a frame fp to a worker specified by target_worker by
 * pushing it into its queue of pushed threads. If the push is successful,
 * the function returns 1, otherwise 0.
 */
int work_try_push(wstream_df_frame_p fp,
		  int target_worker,
		  wstream_df_thread_p cthread,
		  wstream_df_thread_p* wstream_df_worker_threads)
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

  if(fifo_pushback(&wstream_df_worker_threads[target_worker]->push_fifo, fp)) {
    /* Push was successful, update traces and statistics */
    level = mem_lowest_common_level(cthread->worker_id, worker_id_to_cpu(target_worker));
    inc_wqueue_counter(&cthread->pushes_mem[level], 1);

    trace_push(cthread, target_worker, worker_id_to_cpu(target_worker), fp_size, fp);
    return 1;
  }

  /* Push failed, restore owner and update statistics */
  fp->last_owner = curr_owner;
  inc_wqueue_counter(&cthread->pushes_fails, 1);

  return 0;
}

#endif /* ALLOW_PUSHES */

static wstream_df_frame_p steal_hwloc_pu(wstream_df_thread_p thief,
                                         hwloc_obj_t current_obj,
                                         hwloc_obj_t *stolen_from) {
  if (current_obj->type != HWLOC_OBJ_PU)
    return NULL;
  wstream_df_thread_p pu_worker_thread =
      (wstream_df_thread_p)current_obj->userdata;
  wstream_df_frame_p frame = cdeque_steal(&pu_worker_thread->work_deque);
  if (!frame) {
    inc_wqueue_counter(&thief->steals_fails, 1);
  } else {
#if CACHE_LAST_STEAL_VICTIM || WQUEUE_PROFILE
    *stolen_from = current_obj;
#endif // CACHE_LAST_STEAL_VICTIM
  }
  return frame;
}

static hwloc_obj_t random_hwloc_pu_in_subtree(wstream_df_thread_p thief,
                                              hwloc_obj_t root) {
  hwloc_obj_t child = root;
  if (root->arity == 0) {
    return root;
  } else {
    unsigned right = prng_nextn(&thief->rands, 2);
    if (right) {
      for (unsigned i = 0; i < root->arity; ++i) {
        child = random_hwloc_pu_in_subtree(thief, root->children[i]);
        if (child->type == HWLOC_OBJ_PU)
          break;
      }
    } else {
      for (unsigned i = root->arity - 1; i < root->arity; --i) {
        child = random_hwloc_pu_in_subtree(thief, root->children[i]);
        if (child->type == HWLOC_OBJ_PU)
          break;
      }
    }
  }
  return child;
}

/* Steal from processing units that are close hierarchically
 * - First visit all the siblings
 * - If nothing found go up in the hierarchy and do the same for all the siblings
 */
static wstream_df_frame_p steal_hwloc_bottom_up(wstream_df_thread_p thief,
                                                hwloc_obj_t current_obj,
                                                hwloc_obj_t stop_at,
                                                hwloc_obj_t *stolen_from) {
  wstream_df_frame_p frame = NULL;
  // Continue going up until we reach the stop node or we successfully stole work
  while (current_obj != stop_at && !frame) {
    // Search for the first node with a siblings in the hierarchy
    while (current_obj != stop_at && !current_obj->next_sibling &&
           !current_obj->prev_sibling) {
      current_obj = current_obj->parent;
    }
    // We found a node with a sibling in the tree
    // Lets explore the siblings processing units
    if (current_obj != stop_at) {
      hwloc_obj_t one_side = current_obj->next_sibling;
      hwloc_obj_t other_side = current_obj->prev_sibling;
      // Visit every siblings hierarchy
      while (!frame && (one_side || other_side)) {
        unsigned random_direction;
        if (one_side && other_side) {
          random_direction = prng_nextn(&thief->rands, 2);
        } else {
          random_direction = one_side != NULL;
        }
        // We select a random pu of the sibling, try to steak work from its queue
        // then we search for work to steal in the tree until we reach the sibling node
        if (random_direction) { // one_side
          hwloc_obj_t sibling_pu = random_hwloc_pu_in_subtree(thief, one_side);
          frame = steal_hwloc_pu(thief, sibling_pu, stolen_from);
          if (!frame)
            frame = steal_hwloc_bottom_up(thief, sibling_pu, one_side, stolen_from);
          one_side = one_side->next_sibling;
        } else { // other_side
          hwloc_obj_t sibling_pu = random_hwloc_pu_in_subtree(thief, other_side);
          frame = steal_hwloc_pu(thief, sibling_pu, stolen_from);
          if (!frame)
            frame = steal_hwloc_bottom_up(thief, sibling_pu, other_side, stolen_from);
          other_side = other_side->prev_sibling;
        }
      }
      current_obj = current_obj->parent;
    }
  }
  return frame;
}

static wstream_df_frame_p
work_steal(wstream_df_thread_p cthread,
           wstream_df_thread_p *wstream_df_worker_threads) {
  wstream_df_frame_p fp = NULL;

#if CACHE_LAST_STEAL_VICTIM
  /* Try to steal from the last victim worker's deque */
  if (cthread->last_steal_from != NULL) {
    wstream_df_thread_p stolen_from =
        (wstream_df_thread_p)cthread->last_steal_from->userdata;
    fp = cdeque_steal(&stolen_from->work_deque);

    if (fp == NULL) {
      inc_wqueue_counter(&cthread->steals_fails, 1);
      cthread->last_steal_from = NULL;
    }
  }
#endif

  if (fp == NULL) {
    // Try to steal from another worker's queue with locality in mind
    hwloc_obj_t stolen_from;
    (void)stolen_from; // To discard warning when WQUEUE_PROFILE is not set
    fp = steal_hwloc_bottom_up(cthread, cthread->cpu, NULL, &stolen_from);
    if (fp == NULL) {
#if CACHE_LAST_STEAL_VICTIM
      cthread->last_steal_from = NULL;
#endif
    } else {
#if WQUEUE_PROFILE
      unsigned common_level =
          level_of_common_ancestor(cthread->cpu, stolen_from);
      inc_wqueue_counter(&cthread->steals_mem[common_level], 1);
#if ALLOW_WQEVENT_SAMPLING
      wstream_df_thread_p stolen_worker_thread =
          (wstream_df_thread_p)stolen_from->userdata;
      trace_steal(cthread, stolen_worker_thread->worker_id,
                  stolen_from->logical_index, fp->size, fp);
#endif
      fp->steal_type = STEAL_TYPE_STEAL;
      fp->last_owner = stolen_from->logical_index;
#endif

#if CACHE_LAST_STEAL_VICTIM
      cthread->last_steal_from = stolen_from;
#endif
    }
  }
  return fp;
}

#if !DISABLE_WQUEUE_LOCAL_CACHE
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
#endif // !DISABLE_WQUEUE_LOCAL_CACHE

static wstream_df_frame_p work_take(wstream_df_thread_p cthread)
{
  wstream_df_frame_p fp = NULL;

  fp = (wstream_df_frame_p)  (cdeque_take (&cthread->work_deque));

  if (fp != NULL)
    inc_wqueue_counter(&cthread->steals_ownqueue, 1);

  return fp;
}

wstream_df_frame_p obtain_work(wstream_df_thread_p cthread,
                               wstream_df_thread_p *wstream_df_worker_threads) {
  wstream_df_frame_p fp = NULL;

#if !DISABLE_WQUEUE_LOCAL_CACHE
  /* Try to obtain frame from the local cache */
  fp = work_cache_take(cthread);
#endif // !DISABLE_WQUEUE_LOCAL_CACHE

  /* Try to obtain frame from the local work deque */
  if (fp == NULL)
    fp = work_take(cthread);

  /* Cache and deque are both empty -> steal */
  if (fp == NULL)
    fp = work_steal(cthread, wstream_df_worker_threads);

#if WQUEUE_PROFILE
  /* A frame pointer could be obtained (locally or via a steal) */
  if (fp != NULL) {
    /* Update memory transfer statistics */
    for (unsigned worker_id = 0; worker_id < wstream_num_workers; worker_id++) {
      if (fp->bytes_cpu_in[worker_id]) {
        unsigned level = level_of_common_ancestor(
            cthread->cpu, wstream_df_worker_threads[worker_id]->cpu);
        inc_wqueue_counter(&cthread->bytes_mem[level],
                           fp->bytes_cpu_in[worker_id]);
        inc_transfer_matrix_entry(cthread->worker_id, worker_id,
                                  fp->bytes_cpu_in[worker_id]);
      }
    }
  }
#endif // WQUEUE_PROFILE

  return fp;
}