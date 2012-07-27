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

#include "atomic-defs-c11.h"
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
  cbuffer_free (atomic_load_explicit (&cdeque->cbuffer, relaxed));
  free (cdeque);
}

/* Print the elements in the deque CDEQUE.  */
static inline void
print_cdeque (cdeque_p cdeque)
{
  size_t i;
  size_t bottom = atomic_load_explicit (&cdeque->bottom, relaxed);
  size_t top = atomic_load_explicit (&cdeque->top, acquire);
  cbuffer_p buffer = atomic_load_explicit (&cdeque->cbuffer, relaxed);
  for (i = top; i < bottom; i++)
    printf ("%p,", cbuffer_get (buffer, i, relaxed));
  printf ("\n");
}

#if !NO_INLINE_CDEQUE
#define CDEQUE_API static inline
#include "cdeque-c11.c.h"
#else
#define CDEQUE_API
CDEQUE_API void cdeque_push_bottom (cdeque_p, wstream_df_type);
CDEQUE_API wstream_df_type cdeque_take (cdeque_p);
CDEQUE_API wstream_df_type cdeque_steal (cdeque_p);
#endif

#endif
