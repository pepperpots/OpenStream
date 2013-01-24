#ifndef __SLAB_ALLOCATOR_H_
#define __SLAB_ALLOCATOR_H_

#define __slab_align 64
#define __slab_min_size 6 // 64 -- smallest slab
#define __slab_max_size 20 // 262144 = 256KB -- biggest slab
#define __slab_alloc_size 21 // 1048576 = 1MB -- amount of memory that should be allocated in one go

typedef struct slab
{
  struct slab * next;

} slab_t, *slab_p;

static __thread slab_p slab_alloc_free_pool[__slab_max_size + 1];

static inline unsigned int
get_slab_index (unsigned int size)
{
  unsigned int leading_nonzero_pos = 32 - __builtin_clz (size) - 1;
  unsigned int size_2 = 1 << leading_nonzero_pos;
  unsigned int idx = leading_nonzero_pos;

  if ((size ^ size_2) != 0)
    idx = idx + 1;
  if (idx < __slab_min_size)
    idx = __slab_min_size;

  return idx;
}

static inline void
slab_refill (unsigned int idx)
{
  const unsigned int num_slabs = 1 << (__slab_alloc_size - idx);
  const unsigned int slab_size = 1 << idx;
  unsigned int i;
  slab_p s;

  assert (!posix_memalign ((void **) &slab_alloc_free_pool[idx],
			   __slab_align, 1 << __slab_alloc_size));

  s = slab_alloc_free_pool[idx]; // avoid useless warning;
  for (i = 0; i < num_slabs - 1; ++i)
    {
      s->next = (slab_p) (((char *) s) + slab_size);
      s = s->next;
    }
  s->next = NULL;
}

static inline void *
slab_alloc (unsigned int size)
{
  unsigned int idx = get_slab_index (size);
  void *res;

  if (idx > __slab_max_size)
    {
      assert (!posix_memalign ((void **) &res, __slab_align, size));
      return res;
    }

  if (slab_alloc_free_pool[idx] == NULL)
    slab_refill (idx);

  res = (void *) slab_alloc_free_pool[idx];
  slab_alloc_free_pool[idx] = slab_alloc_free_pool[idx]->next;

  return res;
}

static inline void
slab_free (void *e, unsigned int size)
{
  slab_p elem = (slab_p) e;
  unsigned int idx = get_slab_index (size);

  if (idx > __slab_max_size)
    {
      free (e);
    }
  else
    {
      elem->next = slab_alloc_free_pool[idx];
      slab_alloc_free_pool[idx] = elem;
    }
}

static inline void
slab_init_allocator ()
{
  int i;

  for (i = 0; i < __slab_max_size + 1; ++i)
    slab_alloc_free_pool[i] = NULL;
}


#undef __slab_align
#undef __slab_min_size
#undef __slab_max_size
#undef __slab_alloc_size

#if !NO_SLAB_ALLOCATOR
#  define wstream_alloc(PP,A,S)			\
  *(PP) = slab_alloc ((S))
#  define wstream_free(P,S)			\
  slab_free ((P), (S))
#  define wstream_init_alloc()			\
  slab_init_allocator ()
#else
#  define wstream_alloc(PP,A,S)			\
  assert (!posix_memalign ((void **) (PP), (A), (S)))
#  define wstream_free(P,S)			\
  free ((P))
#  define wstream_init_alloc()
#endif


#endif
