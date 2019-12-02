#ifndef NUMA_H
#define NUMA_H

extern "C" {

#include "alloc.h"
#include "wstream_df.h"

typedef struct __attribute__ ((aligned (64))) wstream_df_numa_node
{
  slab_cache_t slab_cache;
  wstream_df_thread_p leader;
  wstream_df_thread_p workers[MAX_CPUS];
  unsigned int num_workers;
  int id;
  unsigned long long frame_bytes_allocated;
} wstream_df_numa_node_t, *wstream_df_numa_node_p;

int numa_nodes_init(void);

static inline void numa_node_add_thread(wstream_df_numa_node_p node, wstream_df_thread_p thread)
{
  thread->slab_cache = &node->slab_cache;
  thread->numa_node = node;

  if(!node->leader || node->leader->worker_id > thread->worker_id)
    node->leader = thread;

  node->workers[node->num_workers] = thread;
  node->num_workers++;
}

int numa_nodes_init(void);
wstream_df_numa_node_p numa_node_by_id(unsigned int id);
wstream_df_thread_p leader_of_numa_node_id(int node_id);

}

#endif
