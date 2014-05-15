#ifndef __SLAB_ALLOCATOR_H_
#define __SLAB_ALLOCATOR_H_

#include <assert.h>
#include <stdlib.h>
#include <numaif.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include "config.h"

#define SLAB_NUMA_CHUNK_SIZE 65536
#define SLAB_NUMA_MAX_ADDR_AT_ONCE 1000

static inline int slab_get_numa_node(void* address, unsigned int size)
{
	void* addr_aligned = (void*)(((unsigned long)address) & ~(0xfff));
	void* addr_check[SLAB_NUMA_MAX_ADDR_AT_ONCE];
	int on_nodes[SLAB_NUMA_MAX_ADDR_AT_ONCE];
	int nodes[MAX_NUMA_NODES];

	memset(nodes, 0, MAX_NUMA_NODES*sizeof(int));

	for(unsigned int i = 0; i < size; i += SLAB_NUMA_CHUNK_SIZE*SLAB_NUMA_MAX_ADDR_AT_ONCE) {
		int num_addr = 0;

		for(num_addr = 0;
		    num_addr < SLAB_NUMA_MAX_ADDR_AT_ONCE &&
			    (i + num_addr*SLAB_NUMA_CHUNK_SIZE) < size;
		    num_addr++)
		{
			addr_check[num_addr] = (void*)(((unsigned long)addr_aligned)+i
						       + num_addr*SLAB_NUMA_CHUNK_SIZE);
		}

		if(move_pages(0, num_addr, addr_check, NULL, on_nodes, 0)) {
			fprintf(stderr, "Could not get node info\n");
			exit(1);
		}

		for(int j = 0; j < num_addr; j++)
			if(on_nodes[j] >= 0)
				nodes[on_nodes[j]]++;
	}

	int max_node = -1;
	int max_size = -1;
	for(int i = 0; i < MAX_NUMA_NODES; i++) {
		if(max_node == -1 || max_size < nodes[i]) {
			max_node = i;
			max_size = nodes[i];
		}
	}

	if(max_node < 0) {
	  fprintf(stderr, "Could not determine node of %p\n", address);
	}

	return max_node;
}

#define __slab_max_slabs 64
#define __slab_align 64
#define __slab_min_size 6 // 64 -- smallest slab
#define __slab_max_size 21 // 2MiB -- biggest slab
#define __slab_alloc_size 21 // 2MiB -- amount of memory that should be allocated in one go

#if !NO_SLAB_ALLOCATOR
  #define WSTREAM_DF_THREAD_SLAB_FIELDS \
   slab_cache_p slab_cache
#else
  #define WSTREAM_DF_THREAD_SLAB_FIELDS
#endif

typedef struct slab
{
  struct slab * next;

} slab_t, *slab_p;

typedef struct slab_metainfo {
	int allocator_id;
	int max_initial_writer_id;
	unsigned int max_initial_writer_size;
	unsigned int size;
	int numa_node;
} slab_metainfo_t, *slab_metainfo_p;

#define ROUND_UP(x,y) (((x)+(y)-1)/(y))
#define __slab_metainfo_size (ROUND_UP(sizeof(slab_metainfo_t), __slab_align)*__slab_align)

typedef struct slab_cache {
  slab_p slab_free_pool[__slab_max_size + 1];
  pthread_spinlock_t locks[__slab_max_size + 1];

  unsigned long long slab_bytes;
  unsigned long long slab_refills;
  unsigned long long slab_allocations;
  unsigned long long slab_frees;
  unsigned long long slab_freed_bytes;
  unsigned long long slab_hits;
  unsigned long long slab_toobig;
  unsigned long long slab_toobig_frees;
  unsigned long long slab_toobig_freed_bytes;
  unsigned int allocator_id;
  unsigned int num_objects;
} slab_cache_t, *slab_cache_p;

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

static inline slab_metainfo_p
slab_metainfo(void* ptr)
{
  return (slab_metainfo_p)(((char*)ptr) - __slab_metainfo_size);
}

static inline unsigned int
slab_allocator_of(void* ptr)
{
  slab_metainfo_p metainfo = slab_metainfo(ptr);
  return metainfo->allocator_id;
}

static inline unsigned int
slab_is_fresh(void* ptr)
{
  slab_metainfo_p metainfo = slab_metainfo(ptr);
  return (metainfo->max_initial_writer_id == -1);
}

static inline unsigned int
slab_max_initial_writer_of(void* ptr)
{
  slab_metainfo_p metainfo = slab_metainfo(ptr);
  return metainfo->max_initial_writer_id;
}

static inline unsigned int
slab_max_initial_writer_size_of(void* ptr)
{
  slab_metainfo_p metainfo = slab_metainfo(ptr);
  return metainfo->max_initial_writer_size;
}

static inline void
slab_set_max_initial_writer_of(void* ptr, int miw, unsigned int size)
{
  slab_metainfo_p metainfo = slab_metainfo(ptr);
  metainfo->max_initial_writer_id = miw;
  metainfo->max_initial_writer_size = size;
}

static inline int
slab_numa_node_of(void* ptr)
{
  slab_metainfo_p metainfo = slab_metainfo(ptr);
  return metainfo->numa_node;
}

static inline size_t
slab_size_of(void* ptr)
{
  slab_metainfo_p metainfo = slab_metainfo(ptr);
  return metainfo->size;
}

static inline void
slab_set_numa_node_of(void* ptr, int node)
{
  slab_metainfo_p metainfo = slab_metainfo(ptr);
  metainfo->numa_node = node;
}

static inline void
slab_update_numa_node_of(void* ptr)
{
  slab_metainfo_p metainfo = slab_metainfo(ptr);

  if(get_slab_index(metainfo->size) <= __slab_max_size)
	  metainfo->numa_node = slab_get_numa_node((char*)ptr, metainfo->size);
}

static inline void
slab_metainfo_init(slab_cache_p slab_cache, slab_metainfo_p metainfo)
{
      metainfo->allocator_id = slab_cache->allocator_id;
      metainfo->max_initial_writer_id = -1;
      metainfo->max_initial_writer_size = 0;
      metainfo->numa_node = -1;
}

static inline void
slab_refill (slab_cache_p slab_cache, unsigned int idx)
{
  unsigned int num_slabs = 1 << (__slab_alloc_size - idx);
  const unsigned int slab_size = 1 << idx;
  unsigned int i;
  slab_p s;
  void* alloc = NULL;
  slab_metainfo_p metainfo;

  if(num_slabs > __slab_max_slabs)
	  num_slabs = __slab_max_slabs;

  int alloc_size = num_slabs * (slab_size + __slab_metainfo_size);


  assert (!posix_memalign (&alloc,
			   __slab_align,
			   alloc_size));

pthread_spin_lock(&slab_cache->locks[idx]);
  slab_p new_head = (slab_p)(((char*)alloc) + __slab_metainfo_size);

  s = new_head; //slab_cache->slab_free_pool[idx]; // avoid useless warning;
  for (i = 0; i < num_slabs; ++i)
    {
      metainfo = slab_metainfo(s);
      slab_metainfo_init(slab_cache, metainfo);

      if(i == num_slabs-1) {
	s->next = slab_cache->slab_free_pool[idx];
      } else {
	s->next = (slab_p) (((char *) s) + slab_size + __slab_metainfo_size);
	s = s->next;
      }
    }

  slab_cache->slab_refills++;
  slab_cache->slab_bytes += num_slabs * slab_size;

  slab_cache->num_objects += num_slabs;
  slab_cache->slab_free_pool[idx] = new_head;
pthread_spin_unlock(&slab_cache->locks[idx]);
}

static inline void *
slab_alloc (slab_cache_p slab_cache, unsigned int size)
{
  unsigned int idx = get_slab_index (size);
  void *res;
  slab_metainfo_p metainfo;

  slab_cache->slab_allocations++;

  if (idx > __slab_max_size)
    {
      slab_cache->slab_toobig++;
      assert (!posix_memalign ((void **) &res, __slab_align, size + __slab_metainfo_size));
      metainfo = res;
      slab_metainfo_init(slab_cache, metainfo);
      metainfo->size = size;
      return (((char*)res) + __slab_metainfo_size);
    }

retry:
  pthread_spin_lock(&slab_cache->locks[idx]);

  if (slab_cache->slab_free_pool[idx] == NULL) {
	  pthread_spin_unlock(&slab_cache->locks[idx]);
	  slab_refill (slab_cache, idx);
	  goto retry;
  } else {
    slab_cache->slab_hits++;
  }

  res = (void *) slab_cache->slab_free_pool[idx];
  slab_cache->slab_free_pool[idx] = slab_cache->slab_free_pool[idx]->next;

  pthread_spin_unlock(&slab_cache->locks[idx]);

  metainfo = slab_metainfo(res);
  metainfo->size = size;
  slab_cache->num_objects--;

  return res;
}

static inline void
slab_free (slab_cache_p slab_cache, void *e)
{
  slab_p elem = (slab_p) e;
  slab_metainfo_p metainfo = slab_metainfo(e);
  unsigned int idx = get_slab_index (metainfo->size);

  if (idx > __slab_max_size)
    {
      slab_cache->slab_toobig_frees++;
      slab_cache->slab_toobig_freed_bytes += metainfo->size;
      free (slab_metainfo(e));
    }
  else
    {
      pthread_spin_lock(&slab_cache->locks[idx]);
      elem->next = slab_cache->slab_free_pool[idx];
      slab_cache->slab_free_pool[idx] = elem;
      pthread_spin_unlock(&slab_cache->locks[idx]);

      slab_cache->slab_frees++;
      slab_cache->slab_freed_bytes += metainfo->size;
      slab_cache->num_objects++;
    }
}

static inline void
slab_init_allocator (slab_cache_p slab_cache, unsigned int allocator_id)
{
  int i;

  for (i = 0; i < __slab_max_size + 1; ++i)
    slab_cache->slab_free_pool[i] = NULL;

  slab_cache->allocator_id = allocator_id;
  slab_cache->slab_bytes = 0;
  slab_cache->slab_refills = 0;
  slab_cache->slab_allocations = 0;
  slab_cache->slab_frees = 0;
  slab_cache->slab_freed_bytes = 0;
  slab_cache->slab_hits = 0;
  slab_cache->slab_toobig = 0;
  slab_cache->slab_toobig_frees = 0;
  slab_cache->slab_toobig_freed_bytes = 0;
  slab_cache->num_objects = 0;

  for (i = 0; i < __slab_max_size + 1; ++i)
    pthread_spin_init(&slab_cache->locks[i], PTHREAD_PROCESS_PRIVATE);
}


#undef __slab_align
#undef __slab_min_size
#undef __slab_max_size
#undef __slab_alloc_size

#if !NO_SLAB_ALLOCATOR
#  define wstream_alloc(SLAB, PP,A,S)			\
  *(PP) = slab_alloc ((SLAB), (S))
#  define wstream_free(SLAB, P)			\
  slab_free ((SLAB), (P))
#  define wstream_init_alloc(SLAB, ID)		\
  slab_init_allocator (SLAB, ID)
#  define wstream_allocator_of(P)		\
  slab_allocator_of(P)
#  define wstream_numa_node_of(P)		\
  slab_numa_node_of(P)
#  define wstream_size_of(P)		\
  slab_size_of(P)
#  define wstream_is_fresh(P)		\
  slab_is_fresh(P)
#  define wstream_max_initial_writer_of(P)		\
  slab_max_initial_writer_of(P)
#  define wstream_max_initial_writer_size_of(P)		\
  slab_max_initial_writer_size_of(P)
#  define wstream_set_max_initial_writer_of(P, MIW, SZ)	\
	slab_set_max_initial_writer_of(P, MIW, SZ)
#  define wstream_update_numa_node_of(P) \
  slab_update_numa_node_of(P)
#else
#  define wstream_alloc(SLAB, PP,A,S)			\
  assert (!posix_memalign ((void **) (PP), (A), (S)))
#  define wstream_free(SLAB, P)			\
  free ((P))
#  define wstream_init_alloc(SLAB, ID)
#  define wstream_allocator_of(P) (-1)
#  define wstream_numa_node_of(P) (-1)
#  define wstream_size_of(P) (-1)
#  define wstream_is_fresh(P) (0)
#  define wstream_max_initial_writer_of(P) (0)
#  define wstream_max_initial_writer_size_of(P) (0)
#  define wstream_set_max_initial_writer_of(P, MIW, SZ) do { } while(0)
#  define wstream_update_numa_node_of(P) do { } while(0)
#endif


#endif
