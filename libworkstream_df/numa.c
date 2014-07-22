#include "numa.h"
#include "interleave.h"

static wstream_df_numa_node_p wstream_df_numa_nodes[MAX_NUMA_NODES];

static inline void numa_node_init(wstream_df_numa_node_p node, int node_id)
{
  slab_init_allocator(&node->slab_cache, node_id);
  /* slab_warmup_size (&node->slab_cache, 524288, 16, node_id); */
  node->leader = NULL;
  node->num_workers = 0;
  node->id = node_id;
  node->frame_bytes_allocated = 0;
}

int numa_nodes_init(void)
{
  size_t size = ROUND_UP(sizeof(wstream_df_numa_node_t), PAGE_SIZE);
  void* ptr;

  for(int i = 0; i < MAX_NUMA_NODES; i++) {
    if(posix_memalign(&ptr, PAGE_SIZE, size))
      wstream_df_fatal("Cannot allocate numa node structure");

    wstream_df_alloc_on_node(ptr, size, i);

    wstream_df_numa_nodes[i] = ptr;
    numa_node_init(wstream_df_numa_nodes[i], i);
  }

  return 0;
}

wstream_df_numa_node_p numa_node_by_id(unsigned int id)
{
  assert(id < MAX_NUMA_NODES);
  return wstream_df_numa_nodes[id];
}

wstream_df_thread_p leader_of_numa_node_id(int node_id)
{
  wstream_df_numa_node_p node;

  if(node_id == -1)
    return NULL;

  node = numa_node_by_id(node_id);
  return node->leader;
}
