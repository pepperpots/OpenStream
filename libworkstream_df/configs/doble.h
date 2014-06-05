#ifndef CONFIG_H
#define CONFIG_H

#include <assert.h>

//#define _WSTREAM_DF_DEBUG 1
//#define _PAPI_PROFILE
//#define _PRINT_STATS
#define _WS_NO_YIELD_SPIN

#define TRACE_QUEUE_STATS

#define WSTREAM_DF_DEQUE_LOG_SIZE 8
#define WSTREAM_STACK_SIZE 1 << 16

#define MAX_CPUS 64
#define MAX_NUMA_NODES 8

#define WQUEUE_PROFILE 1
#define MATRIX_PROFILE "wqueue_matrix.out"

//#define FORCE_SMALL_PAGES
//#define FORCE_HUGE_PAGES

#define PUSH_MIN_MEM_LEVEL 1
#define PUSH_MIN_FRAME_SIZE (64*1024)
#define PUSH_MIN_REL_FRAME_SIZE 1.3
#define NUM_PUSH_SLOTS 128
//#define PUSH_STRATEGY_MAX_WRITER
//#define PUSH_STRATEGY_OWNER
//#define PUSH_STRATEGY_SPLIT_OWNER
//#define PUSH_STRATEGY_SPLIT_OWNER_CHAIN
//#define PUSH_STRATEGY_SPLIT_OWNER_CHAIN_INNER_MW
#define PUSH_STRATEGY_SPLIT_SCORE_NODES
#define ALLOW_PUSHES (NUM_PUSH_SLOTS > 0)

#define DEPENDENCE_AWARE_ALLOC
#define DEPENDENCE_AWARE_ALLOC_EQUAL_RANDOM
//#define DEPENDENCE_AWARE_ALLOC_EQUAL_SEQ

#define TOPOLOGY_AWARE_WORKSTEALING
#define PUSH_EQUAL_RANDOM
//#define PUSH_EQUAL_SEQ

#define REUSE_COPY_ON_NODE_CHANGE
//#define REUSE_DONTCOPY_ON_STEAL

//#define PUSH_ONLY_IF_NOT_STOLEN_AND_CACHE_EMPTY

//#define PAPI_L1
//#define PAPI_L2
//#define PAPI_L3_RAM
//#define PAPI_TLB

#define NUM_PUSH_REORDER_SLOTS 0
#define ALLOW_PUSH_REORDER (ALLOW_PUSHES && NUM_PUSH_REORDER_SLOTS > 0)

#define CACHE_LAST_STEAL_VICTIM 0

#define MAX_WQEVENT_SAMPLES 10000000
#define TRACE_RT_INIT_STATE
#define TRACE_DATA_READS
#define ALLOW_WQEVENT_SAMPLING (MAX_WQEVENT_SAMPLES > 0)
#define MAX_WQEVENT_PARAVER_CYCLES -1
#define NUM_WQEVENT_TASKHIST_BINS 1000

#define WQEVENT_SAMPLING_OUTFILE "events.ost"
#define WQEVENT_SAMPLING_PARFILE "parallelism.gpdata"
#define WQEVENT_SAMPLING_TASKHISTFILE "task_histogram.gpdata"
#define WQEVENT_SAMPLING_TASKLENGTHFILE "task_length.gpdata"

#if defined(PAPI_L1) || defined(PAPI_L2) || defined (PAPI_L3_RAM) || defined(PAPI_TLB)
  #define WS_PAPI_PROFILE
#endif

//#define WS_PAPI_MULTIPLEX

#ifdef WS_PAPI_PROFILE

/* //MISPRED */
/* #define WS_PAPI_NUM_EVENTS 1 */
/* #define WS_PAPI_EVENTS { "PAPI_BR_MSP" } */
/* #define WS_PAPI_UNCORE 0 */
/* #define WS_PAPI_UNCORE_COMPONENT "perf_event_uncore" */

#ifdef PAPI_L1
  //L1
  #define WS_PAPI_NUM_EVENTS 2
  #define WS_PAPI_EVENTS { "PAPI_L1_DCM", "PAPI_l1_DCA" }
  #define WS_PAPI_UNCORE 0
  #define WS_PAPI_UNCORE_COMPONENT "perf_event_uncore"
#endif

#ifdef PAPI_L2
  //L2
  #define WS_PAPI_NUM_EVENTS 2
  #define WS_PAPI_EVENTS { "PAPI_L2_DCM", "PAPI_l2_DCA" }
  #define WS_PAPI_UNCORE 0
  #define WS_PAPI_UNCORE_MASK { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF }
  #define WS_PAPI_UNCORE_COMPONENT "perf_event_uncore"
#endif

#ifdef PAPI_L3_RAM
  // L3
  #define WS_PAPI_NUM_EVENTS 4
  //#define WS_PAPI_EVENTS { "PAPI_L1_DCM", "PAPI_TOT_CYC", "PAPI_L2_DCM" }
  #define WS_PAPI_EVENTS {  /* "PAPI_L2_DCM", "PAPI_L2_DCA", */ /* "PAPI_BR_MSP", "PAPI_STL_ICY", */  "CPU_IO_REQUESTS_TO_MEMORY_IO:LOCAL_CPU_TO_REMOTE_MEM", "CPU_IO_REQUESTS_TO_MEMORY_IO:LOCAL_CPU_TO_LOCAL_MEM", "READ_REQUEST_TO_L3_CACHE:ALL", "L3_CACHE_MISSES:ALL" }
  #define WS_PAPI_UNCORE 1
  #define WS_PAPI_UNCORE_MASK { /* 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF */ 0x0101010101010101, 0x0101010101010101, 0x0101010101010101 , 0x0101010101010101 }
  #define WS_PAPI_UNCORE_COMPONENT "perf_event_uncore"
#endif

#ifdef PAPI_TLB
  #define WS_PAPI_NUM_EVENTS 2
  #define WS_PAPI_EVENTS { "PAPI_TOT_INS", "PAPI_TLB_TL" }
  #define WS_PAPI_UNCORE 0
  #define WS_PAPI_UNCORE_MASK { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF }
  #define WS_PAPI_UNCORE_COMPONENT "perf_event_uncore"
#endif

#define MEM_CACHE_MISS_POS 0 /* Use L1_DCM as cache miss indicator */

#define TRACE_PAPI_COUNTERS
#endif

/* Description of the memory hierarchy */
#define MEM_NUM_LEVELS 7
#define MEM_CACHE_LINE_SIZE 64

#ifndef IN_GCC
#include <string.h>

static inline unsigned int mem_numa_node(unsigned int cpu)
{
	return cpu / 8;
}

static inline unsigned int sibling_wrap(unsigned int cpu, unsigned int sibling, unsigned int num_siblings)
{
	return (cpu - (cpu % num_siblings)) +
		((cpu + (sibling + 1)) % num_siblings);
}

/* 5 levels are defined:
 * 0: 1st level cache, private
 * 1: 2nd level cache, shared by 2 cores
 * 2: 3rd level cache, shared by 8 cores
 * 3: NUMA domain containing 8 cores
 * 4: 32 Cores from NUMA domain at a distance of 1 hop
 * 5: 24 Cores from NUMA domain at a distance of 2 hops
 */
static inline int mem_cores_at_level(unsigned int level, unsigned int cpu)
{
	int cores_at_level[] = {1, 2, 8, 8, 32, 24, 64};
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
				"numa_2hops",
				"machine"};
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

	int node_a = mem_numa_node(a);
	int node_b = mem_numa_node(b);
	int distance = node_distances[node_a][node_b];

	switch(distance) {
		case 10: return 0;
		case 16: return 1;
		case 22: return 2;
	}

	return 0;
}

static inline void mem_score_nodes(int* bytes, int* scores, int num_nodes)
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

	for(int i = 0; i < num_nodes; i++)
		for(int j = 0; j < num_nodes; j++)
			scores[i] += bytes[j]*node_distances[i][j];
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
	int numa_node = mem_numa_node(cpu);

	static int onehop_siblings[8][32] = {
		{ NUMA_NODE_CORES(1), NUMA_NODE_CORES(2), NUMA_NODE_CORES(4), NUMA_NODE_CORES(6) },
		{ NUMA_NODE_CORES(0), NUMA_NODE_CORES(3), NUMA_NODE_CORES(5), NUMA_NODE_CORES(7) },
		{ NUMA_NODE_CORES(0), NUMA_NODE_CORES(3), NUMA_NODE_CORES(4), NUMA_NODE_CORES(6) },
		{ NUMA_NODE_CORES(1), NUMA_NODE_CORES(2), NUMA_NODE_CORES(5), NUMA_NODE_CORES(7) },
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
	int numa_node = mem_numa_node(cpu);

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
			return (a / mem_cores_at_level(level, a)) ==
				(b / mem_cores_at_level(level, b));
		case 4: return (mem_numa_num_hops(a, b) == 1);
		case 5: return (mem_numa_num_hops(a, b) == 2);
		case 6: return 1;
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

static inline int mem_transfer_costs(unsigned int a, unsigned int b)
{
	static int level_transfer_costs[MEM_NUM_LEVELS] = {
		1, 3, 10, 10, 100, 200, 400
	};

	int common_level = mem_lowest_common_level(a, b);
	return level_transfer_costs[common_level];
}

static inline int mem_nth_cache_sibling_at_level(unsigned int level, unsigned int cpu, unsigned int sibling_num)
{
	unsigned int sibling = sibling_wrap(cpu, sibling_num, mem_cores_at_level(level, cpu));

	assert(cpu < MAX_CPUS);
	assert(level < MEM_NUM_LEVELS);

	return sibling;
}

static inline int mem_nth_machine_sibling(unsigned int cpu, unsigned int sibling_num)
{
	unsigned int sibling = sibling_wrap(cpu, sibling_num, mem_cores_at_level(6, cpu));

	assert(cpu < MAX_CPUS);

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
	else if(level == 6)
		return mem_nth_machine_sibling(cpu, sibling_num);

	assert(0);
}

/* Returns how many steal attempts at a given level should be performed. */
static inline int mem_num_steal_attempts_at_level(unsigned int level)
{
#ifdef TOPOLOGY_AWARE_WORKSTEALING
	int steals_at_level[] = {0, 2, 8, 0, 1, 1, 1};
#else
	/* Configuration for complete random work-stealing */
	int steals_at_level[] = {0, 0, 0, 0, 0, 0, 1};
#endif
	assert(level < MEM_NUM_LEVELS);
	return steals_at_level[level];
}

#ifdef WS_PAPI_PROFILE
#define mem_cache_misses(th) rdtsc()
//((th)->papi_counters[MEM_CACHE_MISS_POS])
#else
#define mem_cache_misses(th) 0
#endif
#endif /* IN_GCC */

#endif
