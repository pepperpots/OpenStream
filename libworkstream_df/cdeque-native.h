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
#include "cbuffer-native.h"


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

/* Print the elements in the deque CDEQUE.  */
static inline void
print_cdeque (cdeque_p cdeque)
{
  size_t i;
  for (i = cdeque->top; i < cdeque->bottom; i++)
    printf ("%p,", cdeque_get (cdeque, i));
  printf ("\n");
}

#if !NO_INLINE_CDEQUE
#define CDEQUE_API static inline
#include "cdeque-native.c.h"
#else
#define CDEQUE_API
CDEQUE_API void cdeque_push_bottom (cdeque_p, wstream_df_type);
CDEQUE_API wstream_df_type cdeque_take (cdeque_p);
CDEQUE_API wstream_df_type cdeque_steal (cdeque_p);
#endif

#endif
