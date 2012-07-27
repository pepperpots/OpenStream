#include <stdatomic.h>
#include <stddef.h>

#include "papi-defs.h"
#include "wstream_df.h"
#include "error.h"
#include "cdeque.h"

/* Push element ELEM to the bottom of the deque CDEQUE. Increase the size
   if necessary.  */
CDEQUE_API void
cdeque_push_bottom (cdeque_p cdeque, wstream_df_type elem)
{
  _PAPI_P0B;

  size_t bottom = atomic_load_explicit (&cdeque->bottom, relaxed);
  size_t top = atomic_load_explicit (&cdeque->top, acquire);

  cbuffer_p buffer = atomic_load_explicit (&cdeque->cbuffer, relaxed);

  XLOG ("cdeque_push_bottom with elem: %d\n", elem);
  if (bottom >= top + buffer->size)
    buffer = cbuffer_grow (buffer, bottom, top, &cdeque->cbuffer);

  cbuffer_set (buffer, bottom, elem, relaxed);
  thread_fence (release);
  atomic_store_explicit (&cdeque->bottom, bottom + 1, relaxed);

  _PAPI_P0E;
}

/* Get one task from CDEQUE for execution.  */
CDEQUE_API wstream_df_type
cdeque_take (cdeque_p cdeque)
{
  _PAPI_P1B;
  size_t bottom, top;
  wstream_df_type task;
  cbuffer_p buffer;

  if (atomic_load_explicit (&cdeque->bottom, relaxed) == 0)
    {
      /* bottom == 0 needs to be treated specially as writing
	 bottom - 1 would wrap around and allow steals to succeed
	 even though they should not. Double-loading bottom is OK
	 as we are the only thread that alters its value. */
      _PAPI_P1E;
      return NULL;
    }

  bottom = atomic_load_explicit (&cdeque->bottom, relaxed) - 1;
  buffer = atomic_load_explicit (&cdeque->cbuffer, relaxed);

  atomic_store_explicit (&cdeque->bottom, bottom, relaxed);
  thread_fence (seq_cst);

  top = atomic_load_explicit (&cdeque->top, relaxed);

  if (bottom < top)
    {
      atomic_store_explicit (&cdeque->bottom, bottom + 1, relaxed);
      _PAPI_P1E;
      return NULL;
    }

  task = cbuffer_get (buffer, bottom, relaxed);

  if (bottom > top)
    {
      _PAPI_P1E;
      return task;
    }

  /* One compare and swap when the deque has one single element.  */
  if (!atomic_compare_exchange_strong_explicit (&cdeque->top, &top, top + 1,
						seq_cst, relaxed))
    task = NULL;
  atomic_store_explicit (&cdeque->bottom, bottom + 1, relaxed);

  _PAPI_P1E;
  return task;
}

/* Steal one elem from deque CDEQUE. return NULL if confict happens.  */
CDEQUE_API wstream_df_type
cdeque_steal (cdeque_p remote_cdeque)
{
  _PAPI_P2B;
  size_t bottom, top;
  wstream_df_type elem;
  cbuffer_p buffer;

  top = atomic_load_explicit (&remote_cdeque->top, acquire);
  thread_fence (seq_cst);
  bottom = atomic_load_explicit (&remote_cdeque->bottom, acquire);

  XLOG ("cdeque_steal with bottom %d, top %d\n", bottom, top);

  if (top >= bottom)
    {
      _PAPI_P2E;
      return NULL;
    }

  buffer = atomic_load_explicit (&remote_cdeque->cbuffer, relaxed);
  elem = cbuffer_get (buffer, top, relaxed);

  if (!atomic_compare_exchange_strong_explicit (&remote_cdeque->top, &top,
						top + 1, seq_cst, relaxed))
    elem = NULL;

  _PAPI_P2E;
  return elem;
}
