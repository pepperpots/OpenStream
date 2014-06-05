#include "reuse.h"
#include "alloc.h"
#include "numa.h"

void __built_in_wstream_df_alloc_view_data_slab(wstream_df_view_p view, size_t size, slab_cache_p slab_cache);
extern __thread wstream_df_thread_p current_thread;

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

  out_view->reuse_consumer_view = in_view;
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

  /* The output clause assumes that it writes to a regular input
   * clause and expects in_view->data to be a valid pointer.
   * FIXME: Do not use local slab cache here */
  __built_in_wstream_df_alloc_view_data_slab(in_view, in_view->horizon, cthread->slab_cache);

  assert(in_view->data);
  assert(reuse_view_has_own_data(in_view));
}

void match_reuse_output_clause_with_input_clause(wstream_df_view_p out_view, wstream_df_view_p in_view)
{
  reuse_view_sanity_check(out_view, in_view);
  out_view->reuse_consumer_view = in_view;

  assert(!is_reuse_view(in_view));
  assert(in_view->data);
  assert(in_view->refcount == 1);

  /* Increment reference count of the output clause */
  __built_in_wstream_df_inc_frame_ref(out_view->owner, 1);
  __built_in_wstream_df_inc_view_ref(out_view->reuse_associated_view, 1);
}

/* The parameter v is a pointer to the fake output view of the task
 * that is to be executed.
 */
void __built_in_wstream_df_reuse_prepare_data(void* v)
{
  int force_reuse = 0;

#ifndef REUSE_COPY_ON_NODE_CHANGE
  force_reuse = 1;
#endif

  wstream_df_thread_p cthread = current_thread;

  /* Fake output view of the task to be executed */
  wstream_df_view_p out_view = v;

  /* Input view of the task to be executed */
  wstream_df_view_p in_view = out_view->reuse_associated_view;

  /* If we don't read from a reuse view, there's nothing to do as
     we already have a local buffer filled with data */
  if(!reuse_view_has_reuse_predecessor(in_view))
    return;

  wstream_df_view_p reuse_data_view = in_view->reuse_data_view;

  /* View of a direct consumer of the task to be executed */
  wstream_df_view_p consumer_view = out_view->reuse_consumer_view;

  if(wstream_is_fresh(reuse_data_view->data) && reuse_data_view->horizon > 10000) {
    wstream_update_numa_node_of(reuse_data_view->data);
    trace_frame_info(cthread, reuse_data_view->data);
    slab_set_max_initial_writer_of(reuse_data_view->data, 0, 0);
  }

  /* Node of the data if reused */
  int reuse_numa_node = wstream_numa_node_of(reuse_data_view->data);

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

    trace_data_write(cthread, in_view->horizon, in_view->data);
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

void __built_in_wstream_df_reuse_update_data(void* v)
{
  /* Fake output view of the task that terminates */
  wstream_df_view_p out_view = v;

  /* Input view of the task that terminates */
  wstream_df_view_p in_view = out_view->reuse_associated_view;

  /* If we don't have a consumer there's nothing to do */
  if(!out_view->reuse_consumer_view)
    return;

  /* For consumer views that are reuse views we need to propagate
   * our producer's view if we have reused its data */
  if(is_reuse_view(out_view->reuse_consumer_view)) {
    if(!reuse_view_has_own_data(in_view)) {
      /* Transfer data ownership to this view */
      in_view->reuse_data_view->data = NULL;

      /* /\* Set our consumer's reuse data view to our predecessor *\/ */
      /* out_view->reuse_consumer_view->reuse_data_view = in_view->reuse_data_view; */

      /* /\* Set out predecessor's consumer to our consumer *\/ */
      /* in_view->reuse_data_view->reuse_consumer_view = out_view->reuse_consumer_view; */

      /* Increase our predecessor's refcount as our consumer potentially
       * reuses it */
      /* __built_in_wstream_df_inc_view_ref(in_view->reuse_data_view, 1); */
      /* __built_in_wstream_df_inc_frame_ref(in_view->reuse_data_view->owner, 1); */

      /* Our consumer has speculatively incremented our reference
       * counter. All the references from the consumer to us have been
       * overwritten, so we can safely decrement out own reference
       * counter. */
      /* __built_in_wstream_df_dec_view_ref(in_view, 1); */
      /* __built_in_wstream_df_dec_frame_ref(in_view->owner, 1); */

      __built_in_wstream_df_dec_view_ref(in_view->reuse_data_view, 1);
      __built_in_wstream_df_dec_frame_ref(in_view->reuse_data_view->owner, 1);
    }
  } else {
    /* The consumer view is an ordinary input view and we just need to
     * copy data to its input buffer. */
    memcpy(out_view->reuse_consumer_view->data, in_view->data, in_view->horizon);

    /* Decrement our own reference counter */
    __built_in_wstream_df_dec_view_ref(in_view, 1);
    __built_in_wstream_df_dec_frame_ref(in_view->owner, 1);
  }
}
