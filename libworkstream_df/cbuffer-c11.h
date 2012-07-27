#ifndef CBUFFER_H
#define CBUFFER_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "atomic-defs-c11.h"

/* #define _DEBUG_THIS_ */

#ifdef _DEBUG_THIS_
#define XLOG(...) printf(__VA_ARGS__)
#else
#define XLOG(...)  /**/
#endif
#define XERR(...) printf(__VA_ARGS__)
#define XSUM(...) printf(__VA_ARGS__)

typedef void *wstream_df_type;
typedef struct cbuffer cbuffer_t, *cbuffer_p;
typedef _Atomic (struct cbuffer *) cbuffer_atomic_p;

typedef _Atomic (wstream_df_type) wstream_df_atomic_type;

struct cbuffer
{
  size_t log_size;
  size_t size;
  size_t modulo_mask;
  /* XXX(nhatle): Should be a flexible array member to match the
     theoretical algorithm better. */
  wstream_df_atomic_type *array;
};

static inline cbuffer_p
cbuffer_alloc (size_t log_size)
{
  void *p;
  cbuffer_p cbuffer;

  if (posix_memalign (&p, 64, sizeof *cbuffer))
    wstream_df_fatal ("Out of memory ...");

  cbuffer = p;
  cbuffer->log_size = log_size;
  cbuffer->size = (1 << log_size);
  cbuffer->modulo_mask = cbuffer->size - 1;
  if (posix_memalign (&p, 64, sizeof *cbuffer->array * cbuffer->size))
    wstream_df_fatal ("Out of memory ...");
  cbuffer->array = p;

  return cbuffer;
}

static inline void
cbuffer_free (cbuffer_p cbuffer)
{
  if (cbuffer && cbuffer->array)
    free (cbuffer->array);
  if (cbuffer)
    free (cbuffer);
}

static inline wstream_df_type
cbuffer_get (cbuffer_p cbuffer, size_t i, memory_order memord)
{
  return atomic_load_explicit (&cbuffer->array[i & cbuffer->modulo_mask],
			       memord);
}

static inline void
cbuffer_set (cbuffer_p cbuffer, size_t i, wstream_df_type elem,
	     memory_order memord)
{
  atomic_store_explicit (&cbuffer->array[i & cbuffer->modulo_mask], elem,
			 memord);
}

static inline void
cbuffer_copy_relaxed (wstream_df_atomic_type *p,
		      wstream_df_atomic_type *q,
		      size_t n)
{
  wstream_df_type x;
  size_t i;
  for (i = 0; i < n; ++i)
    {
      x = atomic_load_explicit (q + i, relaxed);
      atomic_store_explicit (p + i, x, relaxed);
    }
}

static inline cbuffer_p
cbuffer_grow (cbuffer_p old_cbuffer, size_t bottom, size_t top,
	      cbuffer_atomic_p *pnew)
{
  cbuffer_p new_cbuffer = cbuffer_alloc (old_cbuffer->log_size + 1);

  size_t old_buffer_size = old_cbuffer->size;
  size_t old_top_pos = top & old_cbuffer->modulo_mask;
  size_t old_bot_pos = bottom & old_cbuffer->modulo_mask;

  size_t new_top_pos = top & new_cbuffer->modulo_mask;
  size_t new_bot_pos = bottom & new_cbuffer->modulo_mask;

  if (old_top_pos < old_bot_pos)
    {
      /* If no wrap-around for old buffer, new one can't have one if
	 size is doubled.  */
      cbuffer_copy_relaxed (&new_cbuffer->array[new_top_pos],
			    &old_cbuffer->array[old_top_pos],
			    old_bot_pos - old_top_pos);
    }
  else
    {
      /* If old buffer wraps around, then either new one wraps around
	 at same place or it just doesn't.  */

      /* No wrap around in new buffer?  */
      int no_wrap = new_top_pos < new_bot_pos;

      cbuffer_copy_relaxed (&new_cbuffer->array[new_top_pos],
			    &old_cbuffer->array[old_top_pos],
			    old_buffer_size - old_top_pos);
      cbuffer_copy_relaxed (&new_cbuffer->array[no_wrap * old_buffer_size],
			    old_cbuffer->array,
			    old_bot_pos);
    }

  atomic_store_explicit (pnew, new_cbuffer, release);
  thread_fence (release);

  /* XXX(nhatle): Race condition with steal() on freed buffer? */
  cbuffer_free (old_cbuffer);

  return new_cbuffer;
}

static inline void
print_cbuffer (cbuffer_p cbuffer)
{
  size_t i;
  for (i = 0; i < cbuffer->size; i++)
    printf ("%p,", cbuffer_get (cbuffer, i, relaxed));
  printf ("\n");
}

#endif
