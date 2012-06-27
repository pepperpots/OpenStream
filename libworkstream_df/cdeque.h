/* Implementation of the work-stealing algorithm for load-balancing
   from: CHASE, D. AND LEV, Y. 2005. Dynamic circular work-stealing
   deque. In Proceedings of the seventeenth annual ACM symposium on
   Parallelism in algorithms and architectures. SPAA'05.

   http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.170.1097&rep=rep1&type=pdf
   */

#ifndef CDEQUE_H
#define CDEQUE_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "wstream_df.h"
#include "cbuffer.h"


typedef struct cdeque {
  size_t volatile bottom __attribute__ ((aligned (64)));
  size_t volatile top __attribute__ ((aligned (64)));
  cbuffer_p volatile cbuffer __attribute__ ((aligned (64)));
} cdeque_t, *cdeque_p;

/* Get the size/capacity of the deque CDEQUE.  */
static inline size_t
cdeque_size (cdeque_p cdeque)
{
  return cbuffer_size (cdeque->cbuffer);
}

/* Extend the size of the deque CDEQUE, with bottom BOTTOM and top TOP.  */
static inline void
cdeque_grow (cdeque_p cdeque, size_t bottom, size_t top)
{
  cbuffer_grow (&cdeque->cbuffer, bottom, top);
}

/* Set the deque at position POS with element ELEM.  */
static inline void
cdeque_set (cdeque_p cdeque, size_t pos, wstream_df_type elem)
{
  cbuffer_set (cdeque->cbuffer, pos, elem);
}

/* Get the element from the deque CDEQUE at position POS.  */
static inline wstream_df_type
cdeque_get (cdeque_p cdeque, size_t pos)
{
  return cbuffer_get (cdeque->cbuffer, pos);
}

static inline void
cdeque_init (cdeque_p cdeque, size_t log_size)
{
  cdeque->bottom = 0;
  cdeque->top = 0;
  cdeque->cbuffer = cbuffer_alloc (log_size);
}

/* Alloc and initialize the deque with log size LOG_SIZE.  */
static inline cdeque_p
cdeque_alloc (size_t log_size)
{
  cdeque_p cdeque = (cdeque_p) malloc (sizeof (cdeque_t));
  cdeque->bottom = 0;
  cdeque->top = 0;
  cdeque->cbuffer = cbuffer_alloc (log_size);

  return cdeque;
}

/* Dealloc the CDEQUE.  */
static inline void
cdeque_free (cdeque_p cdeque)
{
  cbuffer_free (cdeque->cbuffer);
  free (cdeque);
}

/* Push element ELEM to the bottom of the deque CDEQUE. Increase the size
   if necessary.  */
static inline void
cdeque_push_bottom (cdeque_p cdeque, wstream_df_type elem)
{
  _PAPI_P0B;

  size_t bottom = cdeque->bottom;
  size_t top = cdeque->top;

  XLOG ("cdeque_push_bottom with elem: %d\n", elem);
  if (bottom >= top + cdeque_size (cdeque))
    cdeque_grow (cdeque, bottom, top);

  cdeque_set (cdeque, bottom, elem);

  store_load_fence ();

  cdeque->bottom = bottom + 1;
  _PAPI_P0E;
}

/* Get one task from CDEQUE for execution.  */
static inline wstream_df_type
cdeque_take (cdeque_p cdeque)
{
  _PAPI_P1B;
  size_t bottom, top;
  wstream_df_type task;

  if (cdeque->bottom == 0)
    {
      _PAPI_P1E;
      return NULL;
    }

  bottom = cdeque->bottom - 1;
  cdeque->bottom = bottom;
  store_load_fence ();
  top = cdeque->top;

  if (bottom < top)
    {
      cdeque->bottom = top;
      _PAPI_P1E;
      return NULL;
    }

  task = cdeque_get (cdeque, bottom);

  if (bottom > top)
    {
      _PAPI_P1E;
      return task;
    }
  /* One compare and swap when the deque has one single element.  */
  if (!__sync_bool_compare_and_swap (&cdeque->top, top, top+1))
    task = NULL;

  cdeque->bottom = top + 1;
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

  top = remote_cdeque->top;
  load_load_fence ();
  bottom = remote_cdeque->bottom;
  load_load_fence ();

  XLOG ("cdeque_steal with bottom %d, top %d\n", bottom, top);

  if (top >= bottom)
    {
      _PAPI_P2E;
      return NULL;
    }

  elem = cdeque_get (remote_cdeque, top);
  load_store_fence ();

  if (!__sync_bool_compare_and_swap (&remote_cdeque->top, top, top+1))
    elem = NULL;

  _PAPI_P2E;
  return elem;
}

/* Print the elements in the deque CDEQUE.  */
static inline void
print_cdeque (cdeque_p cdeque)
{
  size_t i;
  for (i = cdeque->top; i < cdeque->bottom; i++)
    printf ("%p,", cdeque_get (cdeque, i));
  printf ("\n");
}

#endif
