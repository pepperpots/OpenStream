#include "papi-defs.h"
#include "wstream_df.h"
#include "error.h"
#include "cdeque.h"
#include "arch.h"
#include "papi-defs.h"

/* Push element ELEM to the bottom of the deque CDEQUE. Increase the size
   if necessary.  */
CDEQUE_API void
cdeque_push_bottom (cdeque_p cdeque, wstream_df_type elem)
{
  _PAPI_P0B;

  size_t bottom = cdeque->bottom;
  size_t top = cdeque->top;

  cbuffer_p buffer = cdeque->cbuffer;

  XLOG ("cdeque_push_bottom with elem: %d\n", elem);
  if (bottom >= top + buffer->size)
    buffer = cbuffer_grow (buffer, bottom, top, &cdeque->cbuffer);

  cbuffer_set (buffer, bottom, elem);
  store_store_fence ();
  cdeque->bottom = bottom + 1;

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

  if (cdeque->bottom == 0)
    {
      /* bottom == 0 needs to be treated specially as writing
	 bottom - 1 would wrap around and allow steals to succeed
	 even though they should not. Double-loading bottom is OK
	 as we are the only thread that alters its value. */
      _PAPI_P1E;
      return NULL;
    }

  buffer = cdeque->cbuffer;
#if LLSC_OPTIMIZATION && defined(__arm__)
  do
    bottom = load_linked (&cdeque->bottom) - 1;
  while (!store_conditional (&cdeque->bottom, bottom));
  /* Force coherence point. */
#else
  bottom = cdeque->bottom - 1;
  cdeque->bottom = bottom;
  store_load_fence ();
#endif

  top = cdeque->top;

  if (bottom < top)
    {
      cdeque->bottom = bottom + 1;
      _PAPI_P1E;
      return NULL;
    }

  task = cbuffer_get (buffer, bottom);

  if (bottom > top)
    {
      _PAPI_P1E;
      return task;
    }

  /* One compare and swap when the deque has one single element.  */
  if (!compare_and_swap (&cdeque->top, top, top+1))
    task = NULL;
  cdeque->bottom = top + 1;

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

  top = remote_cdeque->top;

#if defined(__arm__)
  /* Block until the value read from top has been propagated to all
     other threads. */
  store_load_fence ();
#else
  load_load_fence (top);
#endif

#if LLSC_OPTIMIZATION && defined(__arm__)
  bottom = load_linked (&remote_cdeque->bottom);
  if (!store_conditional (&remote_cdeque->bottom, bottom))
    return NULL;
  /* Force coherence point. */
#else
  bottom = remote_cdeque->bottom;
#endif

  load_load_fence (bottom);

  XLOG ("cdeque_steal with bottom %d, top %d\n", bottom, top);

  if (top >= bottom)
    {
      _PAPI_P2E;
      return NULL;
    }

  buffer = remote_cdeque->cbuffer;
  elem = cbuffer_get (buffer, top);
#if defined(__arm__)
  /* Do not reorder the previous load with the load from the CAS. */
  load_load_fence ((uintptr_t) elem);
#else
  load_store_fence ((uintptr_t) elem);
#endif

  if (!weak_compare_and_swap (&remote_cdeque->top, top, top+1))
    elem = NULL;

  _PAPI_P2E;
  return elem;
}
