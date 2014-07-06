#ifndef BROADCAST_H
#define BROADCAST_H

#include "config.h"

typedef struct wstream_df_broadcast_table {
  volatile void* node_src[MAX_NUMA_NODES];
  size_t refcount;
  int src_node;
} wstream_df_broadcast_table_t, *wstream_df_broadcast_table_p;

static inline void broadcast_table_init(wstream_df_broadcast_table_p bt)
{
	memset(bt, 0, sizeof(*bt));
}

#endif
