#ifndef CONFIG_H
#define CONFIG_H

#include <assert.h>

//#define _WSTREAM_DF_DEBUG 1
//#define _PAPI_PROFILE
//#define _PRINT_STATS

#define WSTREAM_DF_DEQUE_LOG_SIZE 8
#define WSTREAM_STACK_SIZE 1 << 16

#define _PHARAON_MODE

#define MAX_CPUS 64

#define WQUEUE_PROFILE
#define MATRIX_PROFILE "wqueue_matrix.out"

#define NUM_STEAL_ATTEMPTS_L2 2
#define NUM_STEAL_ATTEMPTS_L3 8
#define NUM_STEAL_ATTEMPTS_REMOTE 1

#define PUSH_MIN_MEM_LEVEL 3
#define NUM_PUSH_SLOTS 32
#define NUM_PUSH_ATTEMPTS 16
#define ALLOW_PUSHES (NUM_PUSH_SLOTS > 0)

#define MAX_WQEVENT_SAMPLES 10000000
#define ALLOW_WQEVENT_SAMPLING (MAX_WQEVENT_SAMPLES > 0)
#define MAX_WQEVENT_PARAVER_CYCLES -1

#define WQEVENT_SAMPLING_OUTFILE "events.prv"
#define WQEVENT_SAMPLING_PARFILE "parallelism.gpdata"

//#define WS_PAPI_PROFILE
#define WS_PAPI_MULTIPLEX

#ifdef WS_PAPI_PROFILE
#define WS_PAPI_NUM_EVENTS 2
#define WS_PAPI_EVENTS { PAPI_L1_DCM, PAPI_L2_DCM }
#endif

/* Description of the memory hierarchy */

#define MEM_NUM_LEVELS 6

#ifndef IN_GCC
/* 5 levels are defined:
 * 0: 1st level cache, private
 * 1: 2nd level cache, shared by 2 cores
 * 2: 3rd level cache, shared by 8 cores
 * 3: NUMA domain containing 8 cores
 * 4: 32 Cores from NUMA domain at a distance of 1 hop
 * 5: 24 Cores from NUMA domain at a distance of 2 hops
 */
static inline int mem_cores_at_level(unsigned int level)
{
	int cores_at_level[] = {1, 2, 8, 8, 32, 24};
	assert(level < MEM_NUM_LEVELS);
	return cores_at_level[level];
}

/* Returns the name of a level in the memory hierarchy */
static inline const char* mem_level_name(unsigned int level)
{
	const char* level_names[] = {"same_l1",
				"same_l2",
				"same_l3",
				"same_numa",
				"numa_1hop",
				"numa_2hops"};
	assert(level < MEM_NUM_LEVELS);
	return level_names[level];
}

/* Determines how many NUMA hops two cores are
 * away from each other */
static inline int mem_numa_num_hops(unsigned int a, unsigned int b)
{
	/* Distance matrix as reported by numactl --hardware */
	static int node_distances[8][8] = {
		{10, 16, 16, 22, 16, 22, 16, 22},
		{16, 10, 22, 16, 22, 16, 22, 16},
		{16, 22, 10, 16, 16, 22, 16, 22},
		{22, 16, 16, 10, 22, 16, 22, 16},
		{16, 22, 16, 22, 10, 16, 16, 22},
		{22, 16, 22, 16, 16, 10, 22, 16},
		{16, 22, 16, 22, 16, 22, 10, 16},
		{22, 16, 22, 16, 22, 16, 16, 10}
	};

	int node_a = a / 8;
	int node_b = b / 8;
	int distance = node_distances[node_a][node_b];

	switch(distance) {
		case 10: return 0;
		case 16: return 1;
		case 22: return 2;
	}

	return 0;
}

/* Enumerates all cores of a NUMA node */
#define NUMA_NODE_CORES(numa_node) \
	(numa_node*8+0), \
	(numa_node*8+1), \
	(numa_node*8+2), \
	(numa_node*8+3), \
	(numa_node*8+4), \
	(numa_node*8+5), \
	(numa_node*8+6), \
	(numa_node*8+7)

/* Returns the n-th sibling of a core at a NUMA hop distance of 1. */
static inline int mem_numa_get_1hop_nth_sibling(unsigned int cpu, unsigned int sibling_num)
{
	int numa_node = cpu / 8;

	static int onehop_siblings[8][32] = {
		{ NUMA_NODE_CORES(1), NUMA_NODE_CORES(2), NUMA_NODE_CORES(4), NUMA_NODE_CORES(6) },
		{ NUMA_NODE_CORES(0), NUMA_NODE_CORES(3), NUMA_NODE_CORES(5), NUMA_NODE_CORES(7) },
		{ NUMA_NODE_CORES(0), NUMA_NODE_CORES(3), NUMA_NODE_CORES(4), NUMA_NODE_CORES(6) },
		{ NUMA_NODE_CORES(1), NUMA_NODE_CORES(2), NUMA_NODE_CORES(3), NUMA_NODE_CORES(7) },
		{ NUMA_NODE_CORES(0), NUMA_NODE_CORES(2), NUMA_NODE_CORES(5), NUMA_NODE_CORES(6) },
		{ NUMA_NODE_CORES(1), NUMA_NODE_CORES(3), NUMA_NODE_CORES(4), NUMA_NODE_CORES(7) },
		{ NUMA_NODE_CORES(0), NUMA_NODE_CORES(2), NUMA_NODE_CORES(4), NUMA_NODE_CORES(7) },
		{ NUMA_NODE_CORES(1), NUMA_NODE_CORES(3), NUMA_NODE_CORES(5), NUMA_NODE_CORES(6) }
	};

	return onehop_siblings[numa_node][sibling_num];
}

/* Returns the n-th sibling of a core at a NUMA hop distance of 2. */
static inline int mem_numa_get_2hops_nth_sibling(unsigned int cpu, unsigned int sibling_num)
{
	int numa_node = cpu / 8;

	static int twohops_siblings[8][24] = {
		{ NUMA_NODE_CORES(3), NUMA_NODE_CORES(5), NUMA_NODE_CORES(7)},
		{ NUMA_NODE_CORES(2), NUMA_NODE_CORES(4), NUMA_NODE_CORES(6)},
		{ NUMA_NODE_CORES(1), NUMA_NODE_CORES(5), NUMA_NODE_CORES(7)},
		{ NUMA_NODE_CORES(0), NUMA_NODE_CORES(4), NUMA_NODE_CORES(6)},
		{ NUMA_NODE_CORES(1), NUMA_NODE_CORES(3), NUMA_NODE_CORES(7)},
		{ NUMA_NODE_CORES(0), NUMA_NODE_CORES(2), NUMA_NODE_CORES(6)},
		{ NUMA_NODE_CORES(1), NUMA_NODE_CORES(3), NUMA_NODE_CORES(5)},
		{ NUMA_NODE_CORES(0), NUMA_NODE_CORES(2), NUMA_NODE_CORES(4)}
	};

	return twohops_siblings[numa_node][sibling_num];
}

/* Checks if two cores are siblings at a level */
static inline int mem_level_siblings(unsigned int level, unsigned int a, unsigned int b)
{
	switch(level) {
		case 0:
		case 1:
		case 2:
		case 3:
			return (a / mem_cores_at_level(level)) ==
				(b / mem_cores_at_level(level));
		case 4: return (mem_numa_num_hops(a, b) == 1);
		case 5: return (mem_numa_num_hops(a, b) == 2);
	}

	assert(0);
}

/* Returns the lowest common level in the memory hierarchy between
 * two cores. For example, if a and b share the same L2 cache, then
 * the function returns the number of the level for L2 caches.
 */
static inline int mem_lowest_common_level(unsigned int a, unsigned int b)
{
	int level;

	for(level = 0; level < MEM_NUM_LEVELS; level++)
		if(mem_level_siblings(level, a, b))
			return level;

	assert(0);
}

static inline int mem_nth_cache_sibling_at_level(unsigned int level, unsigned int cpu, unsigned int sibling_num)
{
	unsigned int base = cpu - (cpu % mem_cores_at_level(level));
	unsigned int sibling = base + sibling_num;

	if(sibling == cpu)
		return base + (((sibling_num + 1) % mem_cores_at_level(level)));

	return sibling;
}

/* Returns the n-th sibling of a core at a given level of the memory hierarchy.
 * The core ID returned is guaranteed to be different from the core passed
 * as an argument.
 */
static inline int mem_nth_sibling_at_level(unsigned int level, unsigned int cpu, unsigned int sibling_num)
{
	/* For the cache levels, just determine the base core and add the sibling
	 * For NUMA levels, we have to look up the siblings in a matrix.
	 */
	if(level <= 3)
		return  mem_nth_cache_sibling_at_level(level, cpu, sibling_num);
	else if(level == 4)
		return mem_numa_get_1hop_nth_sibling(cpu, sibling_num);
	else if(level == 5)
		return mem_numa_get_2hops_nth_sibling(cpu, sibling_num);

	assert(0);
}

/* Returns how many steal attempts at a given level should be performed. */
static inline int mem_num_steal_attempts_at_level(unsigned int level)
{
	int steals_at_level[] = {0, 2, 8, 0, 1, 1};
	assert(level < MEM_NUM_LEVELS);
	return steals_at_level[level];
}
#endif /* IN_GCC */

#endif
