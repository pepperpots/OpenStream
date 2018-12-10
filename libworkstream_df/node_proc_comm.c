/**
 * Copyright (C) 2018 Antoniu Pop <antoniu.pop@manchester.ac.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "wstream_df.h"
#include "numa.h"
#include "node_proc_comm.h"

extern __thread wstream_df_thread_p current_thread;
npc_thread_t npc;

void *
worker_npc (void *data)
{
  current_thread = ((wstream_df_thread_p) data);
  wstream_df_thread_p cthread = ((wstream_df_thread_p) data);

  prng_init (&npc.rands, npc.node_id);

  if(cthread->worker_id != 0)
    trace_init(cthread);

  init_wqueue_counters (cthread);

  // Start standard listeners for incoming work or returning results
  // Keep separate to avoid blocking termination of tasks (from
  // returns) when getting too many offloads
  npc_init_comm_listeners (__npc_default_comm_tag_offload_offset, NPC_COMM_TYPE_RECV_FRAME, 1);
  npc_init_comm_listeners (__npc_default_comm_tag_returns_offset, NPC_COMM_TYPE_RECV_RETURN, 1);

  while (!is_termination_reached ())
    {
      // Manage the node-level work queue
      //  - If this is the master node, only send off work, never receive (for now)
      //  - If this is a worker node, then either send work to one of the local threads or to other worker nodes
      //  - Monitor the load balance locally

      npc_handle_outstanding_communications ();

      npc_handle_task_queues ();

      // Offload tasks

      // Issue receive calls for data back

      // Test for completion of returns across all outstanding operations

      // set the data buffers for consumers and tdec/tend as needed

      //if (is_worker_node () && is_termination_reached ())
      //return;

      // Maybe do some work or just ensure we don't keep any on the side...
      if (cthread->own_next_cached_thread != NULL)
	{
	  cdeque_push_bottom (&cthread->work_deque,
			      (wstream_df_type) cthread->own_next_cached_thread);
	  cthread->own_next_cached_thread = NULL;
	}

      sched_yield ();
    }
  return NULL;
}




void
init_npc ()
{
  npc.term_flag = 0;
  npc.remote_queue = new_mpsc_fifo ();

  pthread_mutex_init (&npc.npc_lock, NULL);

#ifdef MPI
  assert (MPI_Init(NULL, NULL) == MPI_SUCCESS);
  MPI_Comm_size(MPI_COMM_WORLD, &npc.num_nodes);
  MPI_Comm_rank(MPI_COMM_WORLD, &npc.node_id);
  npc.next_victim = 1;

  if (is_worker_node())
    {
      assert (MPI_Ibcast (&npc.term_flag, 1, MPI_INT, __npc_default_termination_tag, MPI_COMM_WORLD, &npc.termination_request)
	      == MPI_SUCCESS);
    }
#else
  npc.num_nodes = 1;
  npc.node_id = 0;
  npc.next_victim = 0;
#endif
  npc.outstanding_comms = NULL;
}

void
finalize_npc ()
{
  //fprintf (stderr, "Finished executing on node %llu [%llu]\n", npc.node_id, npc.num_nodes);
#ifdef MPI
  /* The master node decides termination and notifies all workers.  */
  if (!is_worker_node ())
    {
      npc.term_flag = 1;
      assert (MPI_Ibcast (&npc.term_flag, 1, MPI_INT, __npc_default_termination_tag, MPI_COMM_WORLD, &npc.termination_request)
	      == MPI_SUCCESS);
    }

  MPI_Finalize();
#endif
  exit (0);
}


