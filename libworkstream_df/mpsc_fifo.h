#ifndef MPSC_FIFO_H
#define MPSC_FIFO_H

#include <assert.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifndef FIFO_CACHE_ALIGN
#define FIFO_CACHE_ALIGN 64
#endif

#ifndef FIFO_SIZE
#define FIFO_SIZE 512
#endif

#define FIFO_BYTE_SIZE  (FIFO_SIZE * sizeof(void *))


typedef struct {
  /* front and back expressed as numbers of elements, not bytes. */
  alignas(FIFO_CACHE_ALIGN) atomic_size_t front;
  alignas(FIFO_CACHE_ALIGN) atomic_size_t back;

  alignas(FIFO_CACHE_ALIGN) atomic_uintptr_t data[FIFO_SIZE];

}  mpsc_fifo_t, *mpsc_fifo_p;

static inline void
fifo_init(mpsc_fifo_p q)
{
  for (int i = 0; i < FIFO_SIZE; ++i)
    atomic_init (&q->data[i], 0);

  atomic_init(&q->front, 0);
  atomic_init(&q->back, 0);
}

static inline mpsc_fifo_p
new_mpsc_fifo (void)
{
  mpsc_fifo_p fifo;

  if (posix_memalign((void **)&fifo,
		     FIFO_CACHE_ALIGN,
		     sizeof(*fifo)))
    wstream_df_fatal ("posix_memalloc");

  fifo_init(fifo);

  return fifo;
}

/* Multi-producer queue: pushback is called concurrently by multiple
   threads.  */
static inline bool
fifo_pushback (mpsc_fifo_p restrict q, const void * restrict elem)
{
  size_t b, f;

  b = atomic_load_explicit(&q->back, memory_order_acquire);
  f = atomic_load_explicit(&q->front, memory_order_acquire);

  if ((b + 1) % FIFO_SIZE == f)
    return false;

  //atomic_store_explicit (&_q->back, (_b + 1) % FIFO_SIZE, release);
  if (!atomic_compare_exchange_strong_explicit (&q->back, &b, (b + 1) % FIFO_SIZE,
						memory_order_seq_cst, memory_order_relaxed))
    return false;

  atomic_store_explicit (&q->data[b], elem, memory_order_release);

  return true;
}

static inline bool
fifo_popfront (mpsc_fifo_p restrict q, void ** restrict elem)
{
  size_t b, f;
  uintptr_t e;

  b = atomic_load_explicit(&q->back, memory_order_acquire);
  f = atomic_load_explicit(&q->front, memory_order_acquire);

  if (f == b)
    return false;

  e = atomic_load_explicit (&q->data[f], memory_order_acquire);

  /* It is possible that the producer has reserved space but has not
     yet written the data.  */
  if (e != 0)
    *elem = (void *)e;
  else
    return false;

  atomic_store_explicit (&q->data[f], 0, memory_order_release);
  atomic_store_explicit (&q->front,
			 (f + 1) % FIFO_SIZE,
			 memory_order_release);
  return true;
}


#endif  /* MPSC_FIFO_H */
