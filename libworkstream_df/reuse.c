#include "reuse.h"
#include "alloc.h"
#include "numa.h"
#include "broadcast.h"

void __built_in_wstream_df_alloc_view_data_slab(wstream_df_view_p view, size_t size, slab_cache_p slab_cache);
extern __thread wstream_df_thread_p current_thread;

void
__builtin_wstream_df_associate_n_views (size_t n, void *pin, void *pfake)
{
  assert(pin != pfake);

  wstream_df_view_p dummy_in_view = pin;
  wstream_df_view_p dummy_fake_view = pfake;

  wstream_df_view_p in_view_arr = dummy_in_view->next;
  wstream_df_view_p fake_view_arr = dummy_fake_view->next;

  /* Set reuse_associated_view of fake view, such that
     resolve_n_dependencesrecognizes that these views are inout_reuse
     views */
  dummy_in_view->reuse_associated_view = dummy_fake_view;
  dummy_fake_view->reuse_associated_view = dummy_in_view;

  for(size_t i = 0; i < n; i++) {
    in_view_arr[i].reuse_associated_view = &fake_view_arr[i];
    in_view_arr[i].consumer_view = NULL;

    fake_view_arr[i].reuse_associated_view = &in_view_arr[i];
    fake_view_arr[i].consumer_view = NULL;
    fake_view_arr[i].burst = dummy_in_view->horizon;
  }
}

void reuse_view_sanity_check(wstream_df_view_p out_view, wstream_df_view_p in_view)
{
  if(in_view->reached_position != 0 || out_view->burst != in_view->horizon)
    {
      fprintf(stderr,
	      "Reading from an inout_reuse clause or writing to it requires the burst "
	      "output clause to be equal to the reading clause's horizon.\n");
      exit(1);
    }
}

void match_reuse_output_clause_with_reuse_input_clause(wstream_df_view_p out_view, wstream_df_view_p in_view)
{
  reuse_view_sanity_check(out_view, in_view);

  out_view->consumer_view = in_view;
  in_view->reuse_data_view = out_view->reuse_associated_view;

  /* The data pointer will be set at the beginning of the execution as
   * we don't know yet whether we will only reuse the existing buffer
   * or if we will copy the data to a newly allocated buffer */
  in_view->data = NULL;

  /* Increment reference count of direct predecessor */
  __built_in_wstream_df_inc_frame_ref(out_view->reuse_associated_view->owner, 1);
  __built_in_wstream_df_inc_view_ref(out_view->reuse_associated_view, 1);

  assert(out_view->reuse_associated_view->refcount == 2);
  assert(out_view->reuse_associated_view->refcount > 1);
  assert(((wstream_df_frame_p)out_view->reuse_associated_view->owner)->refcount > 1);
}

void match_reuse_input_clause_with_output_clause(wstream_df_view_p out_view, wstream_df_view_p in_view)
{
  wstream_df_thread_p cthread = current_thread;

  reuse_view_sanity_check(out_view, in_view);
  in_view->reuse_data_view = NULL;

#ifdef DEFERRED_ALLOC
  out_view->consumer_view = in_view;
#else
  /* The output clause assumes that it writes to a regular input
   * clause and expects in_view->data to be a valid pointer.
   * FIXME: Do not use local slab cache here */
  __built_in_wstream_df_alloc_view_data_slab(in_view, in_view->horizon, cthread->slab_cache);

  assert(in_view->data);
  assert(reuse_view_has_own_data(in_view));
#endif
}

void match_reuse_output_clause_with_input_clause(wstream_df_view_p out_view, wstream_df_view_p in_view)
{
  reuse_view_sanity_check(out_view, in_view);
  out_view->consumer_view = in_view;

  assert(!is_reuse_view(in_view));
  assert(in_view->refcount == 1);

#ifdef DEFERRED_ALLOC
  if(!in_view->data) {
    match_reuse_output_clause_with_reuse_input_clause(out_view, in_view);
    return;
  }
#else
  assert(in_view->data);
#endif

  /* Increment reference count of the output clause */
  __built_in_wstream_df_inc_frame_ref(out_view->owner, 1);
  __built_in_wstream_df_inc_view_ref(out_view->reuse_associated_view, 1);
}

void __built_in_wstream_df_prepare_peek_data(void* v)
{
#ifdef USE_BROADCAST_TABLES
  wstream_df_thread_p cthread = current_thread;
  wstream_df_view_p peek_view = v;
  wstream_df_broadcast_table_p bt = peek_view->broadcast_table;
  int this_node_id = cthread->numa_node->id;
  void* wait_for_update_val = (void*)1;

  /* Data pointer already assigned. Nothing to do */
  if(peek_view->data)
    return;

  assert(peek_view->broadcast_table);

retry:
  /* If local copy is available: just reuse */
  if(bt->node_src[this_node_id] &&
     bt->node_src[this_node_id] != wait_for_update_val)
    {
      peek_view->data = (void*)bt->node_src[this_node_id];
      assert(slab_numa_node_of(peek_view->data) == this_node_id);
    }
  else
    {
      if(bt->node_src[this_node_id] == wait_for_update_val)
	{
	busy_wait:
	  /* Fail, busy wait until completion and retry */
	  while(bt->node_src[this_node_id] == wait_for_update_val)
	    pthread_yield();

	  goto retry;
	}
      else
	{
	  /* Try to be the worker that creates the local copy */
	  if(__sync_bool_compare_and_swap (&bt->node_src[this_node_id], NULL, wait_for_update_val))
	    {
	      /* Otherwise, allocate own buffer, copy data and update table */
	      __built_in_wstream_df_alloc_view_data_slab(peek_view, peek_view->horizon, cthread->slab_cache);
	      memcpy(peek_view->data, (void*)bt->node_src[bt->src_node], peek_view->horizon);

	      trace_data_read(cthread, 0, peek_view->horizon, 0, (void*)bt->node_src[bt->src_node]);

	      /* Get NUMA node of source data */
	      slab_update_numa_node_of_if_fresh(peek_view->data, cthread, 1);

	      assert(slab_numa_node_of(peek_view->data) == this_node_id);

	      /* Try to update broadcast table with local copy */
	      __sync_bool_compare_and_swap (&bt->node_src[this_node_id], wait_for_update_val, peek_view->data);
	    }
	  else
	    {
	      goto busy_wait;
	    }
	}
  }
#endif
}

void __built_in_wstream_df_prepare_peek_data_vec(size_t n, void* v)
{
#ifdef USE_BROADCAST_TABLES
  wstream_df_view_p fake_view = v;
  wstream_df_view_p view_arr = fake_view->next;

  for(size_t i = 0; i < n; i++)
    __built_in_wstream_df_prepare_peek_data(&view_arr[i]);
#endif
}

/* The parameter v is a pointer to the fake output view of the task
 * that is to be executed.
 */
void __built_in_wstream_df_prepare_data(void* v)
{
  wstream_df_thread_p cthread = current_thread;

  /* Fake output view of the task to be executed */
  wstream_df_view_p out_view = v;
  int force_reuse = 0;

  /* View of a direct consumer of the task to be executed */
  wstream_df_view_p consumer_view = out_view->consumer_view;

  /* Check if this is a deferred allocation between a normal output
   * view and an input view */
  if(!is_reuse_view(out_view) && !out_view->data) {
      /* We're the only producer of the consumer view and the view
       * buffer has not been allocated yet. Allocate consumer's buffer
       * and return.*/
    __built_in_wstream_df_alloc_view_data_slab(consumer_view, consumer_view->horizon, cthread->slab_cache);

    if(is_reuse_view(consumer_view))
      consumer_view->reuse_data_view = NULL;

    out_view->data = consumer_view->data;
  }

  if(!is_reuse_view(out_view))
    return;

#ifndef REUSE_COPY_ON_NODE_CHANGE
  force_reuse = 1;
#endif

  /* Input view of the task to be executed */
  wstream_df_view_p in_view = out_view->reuse_associated_view;

  /* If we don't read from a reuse view, there's nothing to do as
     we already have a local buffer filled with data */
  if(!reuse_view_has_reuse_predecessor(in_view))
    return;

  wstream_df_view_p reuse_data_view = in_view->reuse_data_view;

  slab_update_numa_node_of_if_fresh(reuse_data_view->data, cthread, 1);

  /* Node of the data if reused */
  int reuse_numa_node = slab_numa_node_of(reuse_data_view->data);

  /* Node of the task that is to be executed */
  int this_numa_node = cthread->numa_node->id;

  /* Copy statistics */
  in_view->reuse_count = reuse_data_view->reuse_count;
  in_view->copy_count = reuse_data_view->copy_count;
  in_view->ignore_count = reuse_data_view->ignore_count;

#ifdef REUSE_STOPCOPY
  if(in_view->copy_count + in_view->reuse_count > REUSE_STOPCOPY_CHAIN_LENGTH)
    force_reuse = 1;
#endif

  /* Migrate data if consumer executes on another than the data is
   * located on */
  if(reuse_numa_node != this_numa_node && !force_reuse) {
    __built_in_wstream_df_alloc_view_data_slab(in_view, in_view->horizon, cthread->slab_cache);

    trace_state_change(cthread, WORKER_STATE_RT_INIT);
    trace_data_read(cthread, 0, reuse_data_view->horizon, 0, reuse_data_view->data);

    memcpy(in_view->data, reuse_data_view->data, in_view->horizon);

    trace_data_write(cthread, in_view->horizon, (uint64_t)in_view->data);
    trace_state_restore(cthread);

    in_view->reuse_data_view = NULL;

    /* We don't need our predecessor anymore */
    __built_in_wstream_df_dec_view_ref(reuse_data_view, 1);
    __built_in_wstream_df_dec_frame_ref(reuse_data_view->owner, 1);

    in_view->copy_count++;

    /* if(in_view->horizon > 10000) */
    /*   printf("COPY: %d and %d (%d bytes)\n", reuse_numa_node, this_numa_node, in_view->horizon); */
  } else {
    /* Just reuse the data pointer. There's no need to increment our
     * predecessor's reference counter as this was already done
     * before.*/
    in_view->data = reuse_data_view->data;
    reuse_data_view->data = NULL;

    in_view->reuse_count++;

    /* if(in_view->horizon > 10000) */
    /*   printf("REUSE\n"); */
  }

  assert(in_view->data);
  assert(in_view->refcount > 0);
}

void __built_in_wstream_df_prepare_data_vec(size_t n, void* v)
{
  wstream_df_view_p fake_view = v;
  wstream_df_view_p view_arr = fake_view->next;

  for(size_t i = 0; i < n; i++)
    __built_in_wstream_df_prepare_data(&view_arr[i]);
}

void __built_in_wstream_df_reuse_update_data(void* v)
{
#ifdef DEFERRED_ALLOC
    const int deferred_alloc_enabled = 1;
#else
    const int deferred_alloc_enabled = 0;
#endif

  /* Fake output view of the task that terminates */
  wstream_df_view_p out_view = v;

  /* Input view of the task that terminates */
  wstream_df_view_p in_view = out_view->reuse_associated_view;

  /* If we don't have a consumer there's nothing to do */
  if(!out_view->consumer_view)
    return;

  /* For consumer views that are reuse views we need to propagate
   * our producer's view if we have reused its data */
  if(is_reuse_view(out_view->consumer_view) ||
     (deferred_alloc_enabled && !out_view->consumer_view->data))
  {
    if(!reuse_view_has_own_data(in_view)) {
      /* Transfer data ownership to this view */
      out_view->consumer_view->data = in_view->data;
      in_view->reuse_data_view->data = NULL;

      __built_in_wstream_df_dec_view_ref(in_view->reuse_data_view, 1);
      __built_in_wstream_df_dec_frame_ref(in_view->reuse_data_view->owner, 1);
    } else {
      out_view->consumer_view->data = in_view->data;
    }
  } else {
    /* The consumer view is an ordinary input view with allocated
     * buffer. We just need to copy data to the consumer's input
     * buffer.*/
    memcpy(out_view->consumer_view->data, in_view->data, in_view->horizon);

    /* Decrement our own reference counter */
    __built_in_wstream_df_dec_view_ref(in_view, 1);
    __built_in_wstream_df_dec_frame_ref(in_view->owner, 1);
  }
}

void __built_in_wstream_df_reuse_update_data_vec(size_t n, void* v)
{
  wstream_df_view_p fake_view = v;
  wstream_df_view_p view_arr = fake_view->next;

  for(size_t i = 0; i < n; i++)
    __built_in_wstream_df_reuse_update_data(&view_arr[i]);
}
