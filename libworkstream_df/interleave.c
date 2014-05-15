#include <numaif.h>
#include <stdio.h>
#include "config.h"

int wstream_df_interleave_data(void* p, size_t size)
{
	unsigned long nodemask = 0;
	unsigned long pagemask = 0xFFF;

	for(int i = 0; i < MAX_NUMA_NODES; i++)
		nodemask = (nodemask << 1) | 1;

	if(mbind((void*)((long)p & ~(pagemask)), size, MPOL_INTERLEAVE, &nodemask, MAX_NUMA_NODES, MPOL_MF_MOVE) != 0) {
		fprintf(stderr, "mbind error:\n");
		perror("mbind");
		return 1;
	}

	return 0;
}

int wstream_df_alloc_on_node(void* p, size_t size, int node)
{
	unsigned long nodemask = 1 << node;
	unsigned long pagemask = 0xFFF;

	if(mbind((void*)((long)p & ~(pagemask)), size, MPOL_BIND, &nodemask, MAX_NUMA_NODES+1, MPOL_MF_MOVE) != 0) {
		fprintf(stderr, "mbind error:\n");
		perror("mbind");
		return 1;
	}

	return 0;
}
