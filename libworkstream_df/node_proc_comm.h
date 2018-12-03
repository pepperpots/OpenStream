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

#ifndef NPC_H
#define NPC_H

#include "mpsc_fifo.h"
#include "alloc.h"
#include "prng.h"

#ifdef MPI
#include <mpi.h>
#endif

#include "mpidebug.h"

#define __npc_default_termination_tag 0

#define __npc_default_comm_size_min 10 //  1KiB -- smallest default comm
#define __npc_default_comm_size_max 21 //  2MiB -- biggest default comm
#define __npc_default_comm_tag_offload_offset 100
#define __npc_default_comm_tag_returns_offset 200
static inline unsigned int
get_npc_default_listener_tag (unsigned int size, int offset)
{
  unsigned int leading_nonzero_pos = 32 - __builtin_clz (size) - 1;
  unsigned int size_2 = 1 << leading_nonzero_pos;
  unsigned int idx = leading_nonzero_pos;

  if ((size ^ size_2) != 0)
    idx = idx + 1;
  if (idx < __npc_default_comm_size_min)
    idx = __npc_default_comm_size_min;
  // TODO - this is too strict, will need handling at some point, need to see how
  assert (idx <= __npc_default_comm_size_max);
  return idx + offset;
}

enum npc_comm_type {
  NPC_COMM_TYPE_SEND 		= 0,
  NPC_COMM_TYPE_RECV_FRAME 	= 1,
  NPC_COMM_TYPE_RECV_RETURN 	= 2
};

struct npc_comm_handle;
typedef struct npc_comm_handle
{
  MPI_Request comm_request;
  char *comm_buffer;
  unsigned int size;
  unsigned int tag;
  enum npc_comm_type type;
  struct npc_comm_handle *chain;
  struct npc_comm_handle *prev;
} npc_comm_handle_t, *npc_comm_handle_p;

typedef struct npc_thread
{
  int num_nodes;
  int node_id;

  int next_victim;
  unsigned int rands;

  MPI_Request termination_request;
  int term_flag;

  mpsc_fifo_p remote_queue;
  slab_cache_p slab_cache;
  npc_comm_handle_p outstanding_comms;

  pthread_t posix_thread_id;
} npc_thread_t, *npc_thread_p;


extern npc_thread_t npc;
extern int num_workers;
extern wstream_df_thread_p* wstream_df_worker_threads;
void worker_npc (void *arg);
void init_npc ();
void finalize_npc ();


static inline bool is_worker_node () { return npc.node_id != 0; }
static inline bool npc_terminate () { return npc.term_flag; }
static inline bool is_termination_reached ()
{
#ifdef MPI
  int ret;
  if (is_worker_node ())
    assert (MPI_Test (&npc.termination_request, &ret, MPI_STATUS_IGNORE) == MPI_SUCCESS);
#endif
  return npc_terminate ();
}
static inline npc_comm_handle_p get_new_npc_comm_handle ()
{
  npc_comm_handle_p handle = (npc_comm_handle_p) slab_alloc (NULL, npc.slab_cache, sizeof (npc_comm_handle_t));

  handle->prev = NULL;
  handle->chain = npc.outstanding_comms;
  if (npc.outstanding_comms != NULL)
    npc.outstanding_comms->prev = handle;
  npc.outstanding_comms = handle;
  return handle;
}
static inline npc_comm_handle_p release_npc_comm_handle (npc_comm_handle_p h)
{
  npc_comm_handle_p ret = h->chain;

  if (h->chain != NULL)
    h->chain->prev = h->prev;

  if (h->prev != NULL)
    h->prev->chain = h->chain;
  else
    npc.outstanding_comms = h->chain;

  slab_free (npc.slab_cache, h);
  return ret;
}

/* Communication layer procedures - only used in NPC.  */

static inline void npc_init_comm_listeners (int offset, enum npc_comm_type type, int count)
{
  for (int c = 0; c < count; ++c)
    for (int i = __npc_default_comm_size_min; i <= __npc_default_comm_size_max; ++i)
      {
	npc_comm_handle_p handle = get_new_npc_comm_handle ();
	unsigned int size = 1 << i;
	handle->comm_buffer = slab_alloc (NULL, npc.slab_cache, size);
	handle->size = size;
	handle->tag = get_npc_default_listener_tag (size, offset);
	handle->type = type;
	assert (MPI_Irecv (handle->comm_buffer, handle->size, MPI_CHAR, MPI_ANY_SOURCE,
			   handle->tag, MPI_COMM_WORLD, &handle->comm_request) == MPI_SUCCESS);
      }
}

static inline void npc_reissue_listener (npc_comm_handle_p h)
{
  h->comm_buffer = slab_alloc (NULL, npc.slab_cache, h->size);
  assert (MPI_Irecv (h->comm_buffer, h->size, MPI_CHAR, MPI_ANY_SOURCE,
		     h->tag, MPI_COMM_WORLD, &h->comm_request) == MPI_SUCCESS);
}


/* The following operations are each local to the node and always
   performed by worker threads, not by NPC.  */

/* Prepare task for sending to a remote node.  In a first instance,
   copy everything (frame and input buffers) into a single buffer that
   can be passed on to the remote node.  */
static inline char *
npc_pack_task (wstream_df_frame_p fp)
{
  size_t packed_size = 0;
  char *packed_frame;
  int pos = 0;
  wstream_df_view_p pv, v;
  wstream_df_frame_p pfp;

  // Reserve space for the data size - then store it in the first bytes
  packed_size += 8;
  // Reserve space for the frame
  packed_size += fp->size;
  // Reserve space for the input views
  for (v = fp->input_view_chain; v; v = v->view_chain_next)
    packed_size += v->horizon;

  // Allocate
  //fprintf (stderr, "  allocating total %zu [where frame is %zu]\n", packed_size, fp->size);
  packed_frame = slab_alloc(NULL, npc.slab_cache, packed_size);

  // Store the final packed size in the first 8 bytes
  *((size_t *) (packed_frame + pos)) = packed_size; pos += 8;
  // Copy the frame next - THIS IS JUST A SHALLOW COPY, will need to go through and fix all pointers below
  memcpy ((packed_frame + pos), (char *)fp, fp->size); pos += fp->size;
  pfp = (wstream_df_frame_p) (packed_frame + 8);
  // Setup some data that will be necessary to terminate the task once the remote execution completes
  pfp->bind_mode = BIND_MODE_EXEC_REMOTE;
  pfp->origin_node = npc.node_id;
  pfp->origin_pointer = (void *)fp;

  // Now we need to pack the input buffers
  // From here out we offset-convert all pointers within the frame and views while we also pack the inputs at the end of the packed frame.
  // If there are input views, store the offset in the packed frame
  if (fp->input_view_chain != NULL)
    pfp->input_view_chain = (char *)((size_t)fp->input_view_chain - (size_t)fp);
  // Traverse input view chain - copy data and convert pointers to offsets
  for (v = fp->input_view_chain; v; v = v->view_chain_next)
    {
      //fprintf (stderr, "   Packing data (at %zu) is %d\n", pos, *(int*)v->data);
      memcpy (packed_frame + pos, (char *)v->data, v->horizon);
      pv = (wstream_df_view_p) (((size_t) pfp) + ((size_t)v - (size_t)fp));
      pv->data = pos - 8; // Store the position in the packed frame of the view data
      pos += v->horizon;
      //fprintf (stderr, " In view chain: %zu, (pos : %zu -- horizon %zu)\n", pv->data, pos, v->horizon);
      if (v->view_chain_next)
	pv->view_chain_next = ((char *)v->view_chain_next - (char *) fp);
      else
	pv->view_chain_next = 0;
      // Mark the view data as having 0 references in the packed view
      pv->refcount = 0;

      __built_in_wstream_df_dec_view_ref (v, 1);
    }

  if (fp->output_view_chain != NULL)
    pfp->output_view_chain = (char *)((size_t)fp->output_view_chain - (size_t)fp);
  //fprintf (stderr, " In view chain: %zu\n", pv->view_chain_next);
  for (v = fp->output_view_chain; v; v = v->view_chain_next)
    {
      pv = (wstream_df_view_p) (((size_t) pfp) + ((size_t)v - (size_t)fp));
      pv->data = 0;
      if (v->view_chain_next)
	pv->view_chain_next = ((char *)v->view_chain_next - (char *) fp);
      else
	pv->view_chain_next = 0;
      //fprintf (stderr, " In view chain: %zu\n", pv->view_chain_next);
    }

  return packed_frame;
}

static inline wstream_df_frame_p
npc_unpack_task (char *packed_frame)
{
  wstream_df_frame_p fp;
  wstream_df_view_p v;
  size_t voffset;

  fp = (wstream_df_frame_p) ((size_t)packed_frame + 8);

  // Convert view data offsets back to pointers for input views
  for (voffset = (size_t) fp->input_view_chain; voffset; )
    {
      v = (wstream_df_view_p) ((size_t) fp + voffset);
      v->data = (void *) ((size_t) fp + (size_t) v->data);
      voffset = v->view_chain_next;
      if (v->view_chain_next != 0)
	v->view_chain_next = (wstream_df_view_p) ((size_t) fp + (size_t) v->view_chain_next);
    }
  if (fp->input_view_chain != 0)
    fp->input_view_chain = (wstream_df_view_p) ((size_t) fp + (size_t) fp->input_view_chain);

  // Allocate output buffers for all output views
  for (voffset = fp->output_view_chain; voffset; )
    {
      v = (wstream_df_view_p) ((size_t) fp + voffset);
      v->data = slab_alloc (NULL, npc.slab_cache, v->burst);
      v->owner = 0;
      voffset = v->view_chain_next;
      if (v->view_chain_next != 0)
	v->view_chain_next = (wstream_df_view_p) ((size_t) fp + (size_t) v->view_chain_next);
    }
  if (fp->output_view_chain != 0)
    fp->output_view_chain = (wstream_df_view_p) ((size_t) fp + (size_t) fp->output_view_chain);

  return fp;
}



// Communication layer

// Setup receiver for incoming work

// Setup receiver for incoming data

// Wrapper function - do we need it or just add some small metadata to frames in general? Avoid copies

// Pointers for data buffers (deferred alloc) - need view traversal on both sides when offloading

// Pack small inputs in single message (traverse views and collect all buffers < e.g., 1024 bytes in a single new buffer - likely including the frame)
// This further requires some thought about how tracing data is managed - should there be a link back? what do we send back? just the results?

// Unpack functions - re. offset/pointer conversion procedures

// Add "node_bind" ? or just adjust the types to do the discrimination?

// Work discovery and topology knowledge/management (later)
// Work-stealing at some point
// Tracing, trace aggregation, trace processing, etc.


static inline void
npc_handle_incoming_task (void *data)
{
  wstream_df_frame_p fp = npc_unpack_task (data);
  fp->work_fn (fp); // FIXME-apop: _DOS_NPC this should be pushing the task for exec on a worker

  // Once the task completes, pack and send the data back.
  npc_comm_handle_p handle = get_new_npc_comm_handle ();
  wstream_df_view_p v;
  size_t pos = 0;

  // Reserve space to store the origin frame pointer
  handle->size = sizeof (wstream_df_frame_p);
  // Add space for all outputs
  for (v = fp->output_view_chain; v; v = v->view_chain_next)
    handle->size += v->burst;

  handle->tag = get_npc_default_listener_tag ((unsigned int)handle->size,
					      __npc_default_comm_tag_returns_offset);
  handle->comm_buffer = slab_alloc (NULL, npc.slab_cache, handle->size);
  handle->type = NPC_COMM_TYPE_SEND;

  // In the return packet, the first item is the address of the original frame in the node that created it
  *(void **)(handle->comm_buffer) = fp->origin_pointer;
  pos += sizeof (wstream_df_view_p);

  for (v = fp->output_view_chain; v; v = v->view_chain_next)
    {
      memcpy (handle->comm_buffer + pos, (char *)v->data, v->burst);
      pos += v->burst;
    }
  assert (MPI_Isend (handle->comm_buffer, pos, MPI_CHAR, fp->origin_node,
		     handle->tag, MPI_COMM_WORLD, &handle->comm_request) == MPI_SUCCESS);
  slab_free (npc.slab_cache, data);
}

static inline void
npc_handle_task_return (void *data)
{
  // FP here is the local frame pointer that we've kept for
  // reference. This frame needs now to TEND after we copy the data to
  // output buffers.
  wstream_df_frame_p fp = *(wstream_df_frame_p *)(data);
  wstream_df_view_p v;
  size_t pos = sizeof (wstream_df_view_p);

  for (v = fp->output_view_chain; v; v = v->view_chain_next)
    {
      __built_in_wstream_df_prepare_data (v);
      memcpy ((char *)v->data, (char *)data + pos, v->burst);
      __builtin_ia32_tdecrease_n (v->owner, v->burst, true);
      pos += v->burst;
    }
  __builtin_ia32_tend (fp);
  slab_free (npc.slab_cache, data);
}

static inline void
npc_handle_outstanding_communications ()
{
  npc_comm_handle_p h = npc.outstanding_comms;
  while (h != NULL)
    {
      int ret;
      assert (MPI_Test (&h->comm_request, &ret, MPI_STATUS_IGNORE) == MPI_SUCCESS);

      // If we've successfully completed a communication
      if (ret)
	switch (h->type)
	  {
	  case NPC_COMM_TYPE_SEND:
	    // If we've successfully completed the MPI_Isend, then
	    // free the buffer and recycle the handle.
	    slab_free (npc.slab_cache, h->comm_buffer);
	    h = release_npc_comm_handle (h);
	    continue;
	  case NPC_COMM_TYPE_RECV_FRAME:
	    //fprintf (stderr, "  - [%d] Received a frame \n", npc.node_id);
	    npc_handle_incoming_task (h->comm_buffer);
	    npc_reissue_listener (h);
	    break;
	  case NPC_COMM_TYPE_RECV_RETURN:
	    fprintf (stderr, "  - [%d] Received a return \n", npc.node_id);
	    npc_handle_task_return (h->comm_buffer);
	    npc_reissue_listener (h);
	    break;
	  default:
	    assert (0);
	  }
      h = h->chain;
    }
}






static inline bool
npc_push_task (wstream_df_frame_p fp)
{
  // TODO: this should be a bit more elaborate on decision of whether
  // we want to offload any given task - use the compute ratio heuristic (N-1/N nodes)
  char *packed_frame = npc_pack_task (fp);
  return fifo_pushback (npc.remote_queue, packed_frame);
}

static inline void
npc_handle_task_queues ()
{
  // Traverse the remoting queue and MPI_send those. Add the MPI_Request to the list of outstanding.
  char *packed_frame;
  while (fifo_popfront (npc.remote_queue, &packed_frame))
    {
      npc_comm_handle_p handle = get_new_npc_comm_handle ();
      unsigned int target = prng_nextn (&npc.rands, npc.num_nodes-1)+1;

      handle->size = *((size_t *) (packed_frame));
      handle->tag = get_npc_default_listener_tag ((unsigned int)handle->size,
						  __npc_default_comm_tag_offload_offset);
      handle->comm_buffer = packed_frame;
      handle->type = NPC_COMM_TYPE_SEND;

      assert (MPI_Isend (handle->comm_buffer, handle->size, MPI_CHAR, target,
			 handle->tag, MPI_COMM_WORLD, &handle->comm_request) == MPI_SUCCESS);

      // Now we need to make sure that we are going to keep track of
      // this task until we are notified of completion. Assign to it a unique ID
    }
}
#endif
