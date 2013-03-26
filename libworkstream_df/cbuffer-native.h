#ifndef CBUFFER_H
#define CBUFFER_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "arch.h"

/* #define _DEBUG_THIS_ */

#ifdef _DEBUG_THIS_
#define XLOG(...) printf(__VA_ARGS__)
#else
#define XLOG(...)  /**/
#endif
#define XERR(...) printf(__VA_ARGS__)
#define XSUM(...) printf(__VA_ARGS__)

typedef void *wstream_df_type;
typedef struct cbuffer{
  size_t log_size;
  size_t size;
  size_t modulo_mask;
  wstream_df_type *array;
}cbuffer_t, *cbuffer_p;

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
cbuffer_get (cbuffer_p cbuffer, size_t i)
{
  return cbuffer->array[i & cbuffer->modulo_mask];
}

static inline void
cbuffer_set (cbuffer_p cbuffer, size_t i, wstream_df_type elem)
{
  cbuffer->array[i & cbuffer->modulo_mask] = elem;
}

static inline cbuffer_p
cbuffer_grow (cbuffer_p old_cbuffer, size_t bottom, size_t top,
	      cbuffer_p volatile *pnew)
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
      memcpy (&new_cbuffer->array[new_top_pos],
	      &old_cbuffer->array[old_top_pos],
	      (old_bot_pos - old_top_pos) * sizeof (wstream_df_type));
    }
  else
    {
      /* If old buffer wraps around, then either new one wraps around
	 at same place or it just doesn't.  */

      /* No wrap around in new buffer?  */
      int no_wrap_around = new_top_pos < new_bot_pos;

      memcpy (&new_cbuffer->array[new_top_pos],
	      &old_cbuffer->array[old_top_pos],
	      (old_buffer_size - old_top_pos) * sizeof (wstream_df_type));
      memcpy (&new_cbuffer->array[no_wrap_around * old_buffer_size],
	      old_cbuffer->array,
	      (old_bot_pos) * sizeof (wstream_df_type));
    }

  store_store_fence ();
  *pnew = new_cbuffer;
  store_store_fence ();

  /* XXX(nhatle): Race condition with steal() on freed buffer? */
  cbuffer_free (old_cbuffer);

  return new_cbuffer;
}

static inline void
print_cbuffer (cbuffer_p cbuffer)
{
  size_t i;
  for (i = 0; i < cbuffer->size; i++)
    printf ("%p,", cbuffer_get (cbuffer, i));
  printf ("\n");
}

#endif
