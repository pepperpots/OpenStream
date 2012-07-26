/* Implementation of the work-stealing algorithm for load-balancing
   from: CHASE, D. AND LEV, Y. 2005. Dynamic circular work-stealing
   deque. In Proceedings of the seventeenth annual ACM symposium on
   Parallelism in algorithms and architectures. SPAA'05.

   http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.170.1097&rep=rep1&type=pdf
   */

#ifndef CDEQUE_H
#define CDEQUE_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "wstream_df.h"
#include "cbuffer-c11.h"

typedef struct cdeque cdeque_t, *cdeque_p;

struct cdeque
{
  atomic_size_t bottom __attribute__ ((aligned (64)));
  atomic_size_t top __attribute__ ((aligned (64)));
  cbuffer_atomic_p cbuffer __attribute__ ((aligned (64)));
};

static inline void
cdeque_init (cdeque_p cdeque, size_t log_size)
{
  atomic_init (&cdeque->bottom, 0);
  atomic_init (&cdeque->top, 0);
  atomic_init (&cdeque->cbuffer, cbuffer_alloc (log_size));
}

/* Alloc and initialize the deque with log size LOG_SIZE.  */
static inline cdeque_p
cdeque_alloc (size_t log_size)
{
  cdeque_p cdeque = (cdeque_p) malloc (sizeof (cdeque_t));
  if (cdeque == NULL)
    wstream_df_fatal ("Out of memory ...");
  cdeque_init (cdeque, log_size);
  return cdeque;
}

/* Dealloc the CDEQUE.  */
static inline void
cdeque_free (cdeque_p cdeque)
{
  cbuffer_free (atomic_load_explicit (&cdeque->cbuffer, memory_order_relaxed));
  free (cdeque);
}

/* Push element ELEM to the bottom of the deque CDEQUE. Increase the size
   if necessary.  */
static inline void
cdeque_push_bottom (cdeque_p cdeque, wstream_df_type elem)
{
  _PAPI_P0B;

  size_t bottom = atomic_load_explicit (&cdeque->bottom, memory_order_relaxed);
  size_t top = atomic_load_explicit (&cdeque->top, memory_order_acquire);

  cbuffer_p buffer = atomic_load_explicit (&cdeque->cbuffer,
					   memory_order_relaxed);

  XLOG ("cdeque_push_bottom with elem: %d\n", elem);
  if (bottom >= top + buffer->size)
    cbuffer_grow (buffer, bottom, top, &cdeque->cbuffer);

  cbuffer_set (buffer, bottom, elem, memory_order_relaxed);
  atomic_thread_fence (memory_order_release);
  atomic_store_explicit (&cdeque->bottom, bottom + 1, memory_order_relaxed);

  _PAPI_P0E;
}

/* Get one task from CDEQUE for execution.  */
static inline wstream_df_type
cdeque_take (cdeque_p cdeque)
{
  _PAPI_P1B;
  size_t bottom, top;
  cbuffer_p buffer;
  wstream_df_type task;

  bottom = atomic_load_explicit (&cdeque->bottom, memory_order_relaxed) - 1;
  buffer = atomic_load_explicit (&cdeque->cbuffer, memory_order_relaxed);

  atomic_store_explicit (&cdeque->bottom, bottom, memory_order_relaxed);
  atomic_thread_fence (memory_order_seq_cst);

  top = atomic_load_explicit (&cdeque->top, memory_order_relaxed);

  if (bottom == (size_t) -1 || bottom < top)
    {
      atomic_store_explicit (&cdeque->bottom, bottom + 1, memory_order_relaxed);
      _PAPI_P1E;
      return NULL;
    }

  task = cbuffer_get (buffer, bottom, memory_order_relaxed);

  if (bottom > top)
    {
      _PAPI_P1E;
      return task;
    }

  /* One compare and swap when the deque has one single element.  */
  if (!atomic_compare_exchange_strong_explicit (&cdeque->top, top, top + 1,
						memory_order_seq_cst,
						memory_order_relaxed))
    task = NULL;
  atomic_store_explicit (&cdeque->bottom, bottom + 1, memory_order_relaxed);

  _PAPI_P1E;
  return task;
}

/* Steal one elem from deque CDEQUE. return NULL if confict happens.  */
static inline wstream_df_type
cdeque_steal (cdeque_p remote_cdeque)
{
  _PAPI_P2B;
  size_t bottom, top;
  wstream_df_type elem;
  cbuffer_p buffer;

  top = atomic_load_explicit (&remote_cdeque->top, memory_order_acquire);
  atomic_thread_fence (memory_order_seq_cst);
  bottom = atomic_load_explicit (&remote_cdeque->bottom, memory_order_acquire);

  XLOG ("cdeque_steal with bottom %d, top %d\n", bottom, top);

  if (top >= bottom)
    {
      _PAPI_P2E;
      return NULL;
    }

  buffer = atomic_load_explicit (&remote_cdeque->cbuffer, memory_order_relaxed);
  elem = cbuffer_get (buffer, top, memory_order_relaxed);

  if (!atomic_compare_exchange_strong_explicit (&remote_cdeque->top,
						top, top + 1,
						memory_order_seq_cst,
						memory_order_relaxed))
    elem = NULL;

  _PAPI_P2E;
  return elem;
}

/* Print the elements in the deque CDEQUE.  */
static inline void
print_cdeque (cdeque_p cdeque)
{
  size_t i;
  size_t bottom = atomic_load_explicit (&cdeque->bottom, memory_order_relaxed);
  size_t top = atomic_load_explicit (&cdeque->top, memory_order_acquire);
  cbuffer_p buffer = atomic_load_explicit (&cdeque->cbuffer,
					   memory_order_relaxed);
  for (i = top; i < bottom; i++)
    printf ("%p,", cbuffer_get (buffer, i, memory_order_relaxed));
  printf ("\n");
}

#endif
