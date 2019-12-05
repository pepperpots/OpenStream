#ifndef BROADCAST_H
#define BROADCAST_H

#include "config.h"

#ifdef USE_BROADCAST_TABLES

typedef struct wstream_df_broadcast_table {
  volatile void **node_src;
  size_t refcount;
  int src_node;
} wstream_df_broadcast_table_t, *wstream_df_broadcast_table_p;

static inline void broadcast_table_init(wstream_df_broadcast_table_p bt)
{
  bt->node_src = calloc(num_numa_nodes(), sizeof(*bt->node_src))
  bt->refcount = 0;
  bt->src_node = 0;
}

#endif // USE_BROADCAST_TABLES

#endif
