#ifndef __SLAB_ALLOCATOR_H_
#define __SLAB_ALLOCATOR_H_

#include <assert.h>
#include <stdlib.h>
#include <numaif.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include "config.h"
#include "trace.h"
#include "error.h"
#include "glib_extras.h"

struct wstream_df_thread;

#define SLAB_INITIAL_MEM       (1 << 30)
#define SLAB_GLOBAL_REFILL_MEM (1 << 30)

#define SLAB_NUMA_CHUNK_SIZE 65536
#define SLAB_NUMA_MAX_ADDR_AT_ONCE 1000

#define PAGE_SIZE 4096
#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))

static inline void* align_page_boundary(void* addr)
{
	return (void*)(((long)addr) & ~(PAGE_SIZE-1));
}

static inline size_t round_page_size(size_t size)
{
	return ROUND_UP(size, PAGE_SIZE);
}

static inline size_t size_max2(size_t a, size_t b)
{
  return (a > b) ? a : b;
}

static inline int slab_force_advise_pages(void* addr, size_t size, int advice)
{
    if(madvise(align_page_boundary(addr),
	       round_page_size(size),
	       advice))
    {
	    fprintf(stderr, "Could not disable use of huge pages\n");
	    perror("madvise");
	    return 1;
    }

    return 0;
}

static inline int slab_force_small_pages(void* addr, size_t size)
{
  return slab_force_advise_pages(addr, size, MADV_NOHUGEPAGE);
}

static inline int slab_force_huge_pages(void* addr, size_t size)
{
  return slab_force_advise_pages(addr, size, MADV_HUGEPAGE);
}

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

	/* int size_covered = 0; */
	/* for(int i = 0; i < MAX_NUMA_NODES; i++) */
	/* 	size_covered += nodes[i]*SLAB_NUMA_CHUNK_SIZE; */

	/* if(size > 100000) { */
	/* 	if((100*SLAB_NUMA_CHUNK_SIZE*max_size) / size < 80) { */
	/* 		fprintf(stderr, "Could not determine node of %p: max is %d, covered = %d, size = %d\n", address, SLAB_NUMA_CHUNK_SIZE*max_size, size_covered, size); */
	/* 	} */
	/* } */

	if(max_node < 0)
	  fprintf(stderr, "Could not determine node of %p\n", address);

	return max_node;
}

#define __slab_max_slabs 64
#define __slab_align 64
#define __slab_min_size 6 // 64 -- smallest slab
#define __slab_max_size 21 // 2MiB -- biggest slab
#define __slab_alloc_size 21 // 2MiB -- amount of memory that should be allocated in one go

typedef struct slab
{
  struct slab * next;
  int num_sub_objects;
} slab_t, *slab_p;

typedef struct slab_metainfo {
	int allocator_id;
	int max_initial_writer_id;
	unsigned int max_initial_writer_size;
	unsigned int size;
	int numa_node;
} slab_metainfo_t, *slab_metainfo_p;

#define __slab_metainfo_size (ROUND_UP(sizeof(slab_metainfo_t), __slab_align))

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
  void* free_mem_ptr;
  size_t free_mem_bytes;
  pthread_spinlock_t free_mem_lock;
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

static inline int slab_alloc_memalign(slab_cache_p slab_cache, void** ptr, size_t alignment, size_t size)
{
  size_t align_rest = size % alignment;
  size_t alloc_size = (align_rest) ? size + alignment - align_rest : size;

  pthread_spin_lock(&slab_cache->free_mem_lock);

  if(slab_cache->free_mem_bytes < alloc_size) {
    if(slab_cache->free_mem_bytes)
      printf("wasted %zu bytes\n", slab_cache->free_mem_bytes);

    size_t global_alloc_size = size_max2(alloc_size, SLAB_GLOBAL_REFILL_MEM);

    if(posix_memalign(&slab_cache->free_mem_ptr, alignment, global_alloc_size))
      {
	pthread_spin_unlock(&slab_cache->free_mem_lock);
	return 1;
      }

#ifdef FORCE_HUGE_PAGES
    slab_force_huge_pages(slab_cache->free_mem_ptr, global_alloc_size);
#elif defined(FORCE_SMALL_PAGES)
    slab_force_small_pages(slab_cache->free_mem_ptr, global_alloc_size);
#endif

    slab_cache->free_mem_bytes = global_alloc_size;
  }

  if(((long)slab_cache->free_mem_bytes) % alignment)
    {
      pthread_spin_unlock(&slab_cache->free_mem_lock);
      return 1;
    }

  *ptr = slab_cache->free_mem_ptr;

  slab_cache->free_mem_bytes -= alloc_size;
  slab_cache->free_mem_ptr = ((char*)slab_cache->free_mem_ptr)+alloc_size;

  pthread_spin_unlock(&slab_cache->free_mem_lock);

  return 0;
}

static inline void
slab_refill (struct wstream_df_thread* cthread, slab_cache_p slab_cache, unsigned int idx)
{
  unsigned int num_slabs = 1 << (__slab_alloc_size - idx);
  const unsigned int slab_size = 1 << idx;
  void* alloc = NULL;
  slab_metainfo_p metainfo;

  if(num_slabs > __slab_max_slabs)
	  num_slabs = __slab_max_slabs;

  int alloc_size = num_slabs * (slab_size + __slab_metainfo_size);

  if(cthread)
	  trace_state_change(cthread, WORKER_STATE_RT_ESTIMATE_COSTS);

  assert (!slab_alloc_memalign (slab_cache,
				&alloc,
				__slab_align,
				alloc_size));
  if(cthread)
	  trace_state_restore(cthread);

  if(cthread)
	  trace_state_change(cthread, WORKER_STATE_RT_INIT);

pthread_spin_lock(&slab_cache->locks[idx]);
  slab_p new_head = (slab_p)(((char*)alloc) + __slab_metainfo_size);
  metainfo = slab_metainfo(new_head);
  slab_metainfo_init(slab_cache, metainfo);

  new_head->num_sub_objects = num_slabs;

  slab_cache->slab_refills++;
  slab_cache->slab_bytes += num_slabs * slab_size;

  slab_cache->num_objects += num_slabs;
  new_head->next = slab_cache->slab_free_pool[idx];
  slab_cache->slab_free_pool[idx] = new_head;
pthread_spin_unlock(&slab_cache->locks[idx]);

  if(cthread)
	  trace_state_restore(cthread);
}

static inline void
slab_warmup (slab_cache_p slab_cache, unsigned int idx, unsigned int num_slabs, int node)
{
  const unsigned int slab_size = 1 << idx;
  unsigned int i;
  slab_p s;
  void* alloc = NULL;
  slab_metainfo_p metainfo;

  int alloc_size = num_slabs * (slab_size + __slab_metainfo_size);


  assert (!posix_memalign (&alloc,
			   __slab_align,
			   alloc_size));

  unsigned long nodemask = (1 << node);
  if(mbind((void*)((long)alloc & ~(0xFFF)), alloc_size, MPOL_BIND, &nodemask, MAX_NUMA_NODES+1, MPOL_MF_MOVE) != 0) {
	  fprintf(stderr, "mbind error:\n");
	  perror("mbind");
	  exit(1);
  }

  memset(alloc, 0, alloc_size);

  slab_p new_head = (slab_p)(((char*)alloc) + __slab_metainfo_size);

  s = new_head;
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

  slab_cache->num_objects += num_slabs;
  slab_cache->slab_free_pool[idx] = new_head;
}

static inline void
slab_warmup_size (slab_cache_p slab_cache, unsigned int size, unsigned int num_slabs, int node)
{
  unsigned int idx = get_slab_index (size);
  slab_warmup(slab_cache, idx, num_slabs, node);
}


static inline void *
slab_alloc (struct wstream_df_thread* cthread, slab_cache_p slab_cache, unsigned int size)
{
  unsigned int idx = get_slab_index (size);
  const unsigned int slab_size = 1 << idx;
  void* res;
  slab_p head;
  slab_p new_head;
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
    slab_refill (cthread, slab_cache, idx);
    goto retry;
  } else {
    slab_cache->slab_hits++;
  }

  head = slab_cache->slab_free_pool[idx];

  if(head->num_sub_objects > 1)
    {
      new_head = (slab_p) (((char *) head) + slab_size + __slab_metainfo_size);
      metainfo = slab_metainfo(new_head);
      slab_metainfo_init(slab_cache, metainfo);
      new_head->next = head->next;
      new_head->num_sub_objects = head->num_sub_objects-1;
    }
  else
    {
      new_head = head->next;
    }

  slab_cache->slab_free_pool[idx] = new_head;

  pthread_spin_unlock(&slab_cache->locks[idx]);

  metainfo = slab_metainfo(head);
  metainfo->size = size;
  slab_cache->num_objects--;
  res = head;

  return (void*)res;
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
      elem->num_sub_objects = 1;
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
  slab_cache->free_mem_bytes = SLAB_INITIAL_MEM;

  pthread_spin_init(&slab_cache->free_mem_lock, PTHREAD_PROCESS_PRIVATE);

  if(posix_memalign(&slab_cache->free_mem_ptr, __slab_align, slab_cache->free_mem_bytes) != 0)
    wstream_df_fatal("Could not reserve initial memory for slab allocator\n");

#ifdef FORCE_HUGE_PAGES
  slab_force_huge_pages(slab_cache->free_mem_ptr, slab_cache->free_mem_bytes);
#elif defined(FORCE_SMALL_PAGES)
  slab_force_small_pages(slab_cache->free_mem_ptr, slab_cache->free_mem_bytes);
#endif

  for (i = 0; i < __slab_max_size + 1; ++i)
    pthread_spin_init(&slab_cache->locks[i], PTHREAD_PROCESS_PRIVATE);
}

#undef __slab_align
#undef __slab_min_size
#undef __slab_max_size
#undef __slab_alloc_size

#if !NO_SLAB_ALLOCATOR
#  define wstream_alloc(CTHREAD, SLAB, PP,A,S)	\
	*(PP) = slab_alloc ((CTHREAD), (SLAB), (S))
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
#  define wstream_alloc(CTHREAD, SLAB, PP,A,S)			\
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
