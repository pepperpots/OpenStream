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

  size_t bottom = cdeque->bottom;
  size_t top = cdeque->top;

  XLOG ("cdeque_push_bottom with elem: %d\n", elem);
  if (bottom >= top + cdeque_size (cdeque))
    cdeque_grow (cdeque, bottom, top);

  cdeque_set (cdeque, bottom, elem);

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

  if (cdeque->bottom == 0)
    {
      _PAPI_P1E;
      return NULL;
    }

#if !NO_LLSC_OPTIMIZATION && defined(__arm__)
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

  top = remote_cdeque->top;
#if defined(__arm__)
  /* Block until the value read from top has been propagated to all
     other threads. */
  store_load_fence ();
#else
  load_load_fence (top);
#endif
#if !NO_LLSC_OPTIMIZATION && defined(__arm__)
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

  elem = cdeque_get (remote_cdeque, top);
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
