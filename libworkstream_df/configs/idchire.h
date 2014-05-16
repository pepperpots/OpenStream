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

#define MAX_CPUS 192
#define MAX_NUMA_NODES 24

#define WQUEUE_PROFILE 3
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

#define PUSH_EQUAL_RANDOM
//#define PUSH_EQUAL_SEQ

#define DEPENDENCE_AWARE_ALLOC
#define TOPOLOGY_AWARE_WORKSTEALING
//#define COPY_ON_NODE_CHANGE

//#define PAPI_L1
//#define PAPI_L2
//#define PAPI_L3_RAM

#define NUM_PUSH_REORDER_SLOTS 0
#define ALLOW_PUSH_REORDER (ALLOW_PUSHES && NUM_PUSH_REORDER_SLOTS > 0)

#define CACHE_LAST_STEAL_VICTIM 0

#define MAX_WQEVENT_SAMPLES 10000000
//#define TRACE_RT_INIT_STATE
#define TRACE_DATA_READS
#define ALLOW_WQEVENT_SAMPLING (MAX_WQEVENT_SAMPLES > 0)
#define MAX_WQEVENT_PARAVER_CYCLES -1
#define NUM_WQEVENT_TASKHIST_BINS 1000

#define WQEVENT_SAMPLING_OUTFILE "events.ost"
#define WQEVENT_SAMPLING_PARFILE "parallelism.gpdata"
#define WQEVENT_SAMPLING_TASKHISTFILE "task_histogram.gpdata"
#define WQEVENT_SAMPLING_TASKLENGTHFILE "task_length.gpdata"

#if defined(PAPI_L1) || defined(PAPI_L2) || defined (PAPI_L3_RAM)
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

#define MEM_CACHE_MISS_POS 0 /* Use L1_DCM as cache miss indicator */

#define TRACE_PAPI_COUNTERS
#endif

/* Description of the memory hierarchy */
#define MEM_NUM_LEVELS 8
#define MEM_CACHE_LINE_SIZE 64

#ifndef IN_GCC
#include <string.h>

/* 8 levels are defined:
 * 0: 1st level cache, private
 * 1: 2nd level cache, private
 * 2: 3rd level cache, shared by 8 cores
 * 3: NUMA domain containing 8 cores
 * 4: 8 Cores from NUMA domain at a distance of 1 hop
 * 5: 96 Cores from NUMA domain at a distance of 2 hops
 * 6: 80 Cores from NUMA domain at a distance of 3 hops
 * 7: All 192 cores of the machine
 */
static inline int mem_cores_at_level(unsigned int level, unsigned int cpu)
{
	static int cores_at_level_node_0_15[] = {1, 1, 8, 8, 8, 96, 80, 192};
	static int cores_at_level_node_16_23[] = {1, 1, 8, 8, 8, 80, 96, 192};
	int numa_node = cpu / 8;

	assert(level < MEM_NUM_LEVELS);
	assert(cpu < MAX_CPUS);

	if(numa_node <= 15)
		return cores_at_level_node_0_15[level];
	else
		return cores_at_level_node_16_23[level];
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
				"numa_3hops",
				"machine"};
	assert(level < MEM_NUM_LEVELS);
	return level_names[level];
}

/* Determines how many NUMA hops two cores are
 * away from each other */
static inline int mem_numa_num_hops(unsigned int a, unsigned int b)
{
	/* Distance matrix as reported by numactl --hardware */
	static int node_distances[MAX_NUMA_NODES][MAX_NUMA_NODES] = {
		{10, 50, 65, 65, 65, 65, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79},
		{50, 10, 65, 65, 65, 65, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79},
		{65, 65, 10, 50, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79},
		{65, 65, 50, 10, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79},
		{65, 65, 65, 65, 10, 50, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79},
		{65, 65, 65, 65, 50, 10, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79},
		{65, 65, 65, 65, 65, 65, 10, 50, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65},
		{65, 65, 65, 65, 65, 65, 50, 10, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65},
		{65, 65, 79, 79, 65, 65, 79, 79, 10, 50, 65, 65, 65, 65, 65, 65, 65, 65, 79, 79, 79, 79, 79, 79},
		{65, 65, 79, 79, 65, 65, 79, 79, 50, 10, 65, 65, 65, 65, 65, 65, 65, 65, 79, 79, 79, 79, 79, 79},
		{79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 10, 50, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79},
		{79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 50, 10, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79},
		{65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 10, 50, 65, 65, 79, 79, 79, 79, 65, 65, 79, 79},
		{65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 50, 10, 65, 65, 79, 79, 79, 79, 65, 65, 79, 79},
		{79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 65, 65, 65, 65, 10, 50, 79, 79, 79, 79, 79, 79, 65, 65},
		{79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 65, 65, 65, 65, 50, 10, 79, 79, 79, 79, 79, 79, 65, 65},
		{65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 10, 50, 65, 65, 65, 65, 65, 65},
		{65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 50, 10, 65, 65, 65, 65, 65, 65},
		{79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 65, 65, 10, 50, 65, 65, 65, 65},
		{79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 65, 65, 50, 10, 65, 65, 65, 65},
		{79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 10, 50, 65, 65},
		{79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 50, 10, 65, 65},
		{79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 65, 65, 65, 65, 65, 65, 10, 50},
		{79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 65, 65, 65, 65, 65, 65, 50, 10}
	};

	assert(a < MAX_CPUS);
	assert(a < MAX_CPUS);

	int node_a = a / 8;
	int node_b = b / 8;
	int distance = node_distances[node_a][node_b];

	switch(distance) {
		case 10: return 0;
		case 50: return 1;
		case 65: return 2;
		case 79: return 3;
	}

	return 0;
}

static inline void mem_score_nodes(int* bytes, int* scores, int num_nodes)
{
	/* Distance matrix as reported by numactl --hardware */
	static int node_distances[MAX_NUMA_NODES][MAX_NUMA_NODES] = {
		{10, 50, 65, 65, 65, 65, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79},
		{50, 10, 65, 65, 65, 65, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79},
		{65, 65, 10, 50, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79},
		{65, 65, 50, 10, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79},
		{65, 65, 65, 65, 10, 50, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79},
		{65, 65, 65, 65, 50, 10, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79},
		{65, 65, 65, 65, 65, 65, 10, 50, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65},
		{65, 65, 65, 65, 65, 65, 50, 10, 79, 79, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65},
		{65, 65, 79, 79, 65, 65, 79, 79, 10, 50, 65, 65, 65, 65, 65, 65, 65, 65, 79, 79, 79, 79, 79, 79},
		{65, 65, 79, 79, 65, 65, 79, 79, 50, 10, 65, 65, 65, 65, 65, 65, 65, 65, 79, 79, 79, 79, 79, 79},
		{79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 10, 50, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79},
		{79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 50, 10, 65, 65, 65, 65, 79, 79, 65, 65, 79, 79, 79, 79},
		{65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 10, 50, 65, 65, 79, 79, 79, 79, 65, 65, 79, 79},
		{65, 65, 79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 50, 10, 65, 65, 79, 79, 79, 79, 65, 65, 79, 79},
		{79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 65, 65, 65, 65, 10, 50, 79, 79, 79, 79, 79, 79, 65, 65},
		{79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 65, 65, 65, 65, 50, 10, 79, 79, 79, 79, 79, 79, 65, 65},
		{65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 10, 50, 65, 65, 65, 65, 65, 65},
		{65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 50, 10, 65, 65, 65, 65, 65, 65},
		{79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 65, 65, 10, 50, 65, 65, 65, 65},
		{79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 65, 65, 50, 10, 65, 65, 65, 65},
		{79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 10, 50, 65, 65},
		{79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 65, 65, 65, 65, 50, 10, 65, 65},
		{79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 65, 65, 65, 65, 65, 65, 10, 50},
		{79, 79, 79, 79, 79, 79, 65, 65, 79, 79, 79, 79, 79, 79, 65, 65, 65, 65, 65, 65, 65, 65, 50, 10}
	};

	assert(num_nodes < MAX_NUMA_NODES);

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
	int numa_node = cpu / 8;
	assert(cpu < MAX_CPUS);

	if(numa_node % 2 == 0)
	  return (8*(numa_node+1))+sibling_num;
	else
	  return (8*(numa_node-1))+sibling_num;
}

/* Returns the n-th sibling of a core at a NUMA hop distance of 2. */
static inline int mem_numa_get_2hops_nth_sibling(unsigned int cpu, unsigned int sibling_num)
{
	int numa_node = cpu / 8;

	static int twohops_siblings_node_0_15[16][96] = {
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17)},
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17)},
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19)},
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21)},
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)}
	};

	static int twohops_siblings_node_16_23[8][80] = {
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21)},
		{ NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21)}
	};

	assert(cpu < MAX_CPUS);

	if(numa_node <= 15)
		return twohops_siblings_node_0_15[numa_node][sibling_num];
	else
		return twohops_siblings_node_16_23[numa_node-16][sibling_num];
}

/* Returns the n-th sibling of a core at a NUMA hop distance of 3. */
static inline int mem_numa_get_3hops_nth_sibling(unsigned int cpu, unsigned int sibling_num)
{
	int numa_node = cpu / 8;

	static int threehops_siblings_node_0_15[16][80] = {
		{ NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21)},
		{ NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21)},
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(22), NUMA_NODE_CORES(23)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES(16), NUMA_NODE_CORES(17), NUMA_NODE_CORES(18), NUMA_NODE_CORES(19), NUMA_NODE_CORES(20), NUMA_NODE_CORES(21)},
	};

	static int threehops_siblings_node_16_23[8][96] = {
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15)},
		{ NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 6), NUMA_NODE_CORES( 7), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(14), NUMA_NODE_CORES(15)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13)},
		{ NUMA_NODE_CORES( 0), NUMA_NODE_CORES( 1), NUMA_NODE_CORES( 2), NUMA_NODE_CORES( 3), NUMA_NODE_CORES( 4), NUMA_NODE_CORES( 5), NUMA_NODE_CORES( 8), NUMA_NODE_CORES( 9), NUMA_NODE_CORES(10), NUMA_NODE_CORES(11), NUMA_NODE_CORES(12), NUMA_NODE_CORES(13)},
	};

	assert(cpu < MAX_CPUS);

	if(numa_node <= 15)
		return threehops_siblings_node_0_15[numa_node][sibling_num];
	else
		return threehops_siblings_node_16_23[numa_node-16][sibling_num];
}

/* Checks if two cores are siblings at a level */
static inline int mem_level_siblings(unsigned int level, unsigned int a, unsigned int b)
{
	assert(level < MEM_NUM_LEVELS);
	assert(a < MAX_CPUS);
	assert(b < MAX_CPUS);

	switch(level) {
		case 0:
		case 1:
		case 2:
		case 3:
			return (a / mem_cores_at_level(level, a)) ==
				(b / mem_cores_at_level(level, b));
		case 4: return (mem_numa_num_hops(a, b) == 1);
		case 5: return (mem_numa_num_hops(a, b) == 2);
		case 6: return (mem_numa_num_hops(a, b) == 3);
		case 7: return 1;
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

	assert(a < MAX_CPUS);
	assert(b < MAX_CPUS);

	for(level = 0; level < MEM_NUM_LEVELS; level++)
		if(mem_level_siblings(level, a, b))
			return level;

	assert(0);
}

static inline int mem_transfer_costs(unsigned int a, unsigned int b)
{
	static int level_transfer_costs[MEM_NUM_LEVELS] = {
		1, 3, 10, 10, 100, 200, 400, 800
	};

	assert(a < MAX_CPUS);
	assert(b < MAX_CPUS);

	int common_level = mem_lowest_common_level(a, b);
	return level_transfer_costs[common_level];
}

static inline int mem_nth_cache_sibling_at_level(unsigned int level, unsigned int cpu, unsigned int sibling_num)
{
	unsigned int base = cpu - (cpu % mem_cores_at_level(level, cpu));
	unsigned int sibling = base + sibling_num;

	assert(cpu < MAX_CPUS);
	assert(level < MEM_NUM_LEVELS);

	if(sibling == cpu)
		return base + (((sibling_num + 1) % mem_cores_at_level(level, cpu)));

	return sibling;
}

static inline int mem_nth_machine_sibling(unsigned int cpu, unsigned int sibling_num)
{
	unsigned int sibling = sibling_num % mem_cores_at_level(7, cpu);

	assert(cpu < MAX_CPUS);

	if(sibling == cpu)
		return (sibling_num + 1) % mem_cores_at_level(7, cpu);

	return sibling;
}

/* Returns the n-th sibling of a core at a given level of the memory hierarchy.
 * The core ID returned is guaranteed to be different from the core passed
 * as an argument.
 */
static inline int mem_nth_sibling_at_level(unsigned int level, unsigned int cpu, unsigned int sibling_num)
{
	assert(cpu < MAX_CPUS);
	assert(level < MEM_NUM_LEVELS);

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
		return mem_numa_get_3hops_nth_sibling(cpu, sibling_num);
	else if(level == 7)
		return mem_nth_machine_sibling(cpu, sibling_num);

	assert(0);
}

/* Returns how many steal attempts at a given level should be performed. */
static inline int mem_num_steal_attempts_at_level(unsigned int level)
{
	assert(level < MEM_NUM_LEVELS);
#ifdef TOPOLOGY_AWARE_WORKSTEALING
	int steals_at_level[] = {0, 0, 8, 0, 1, 1, 1, 1};
#else
	/* Configuration for complete random work-stealing */
	int steals_at_level[] = {0, 0, 0, 0, 0, 0, 0, 1};
#endif
	assert(level < MEM_NUM_LEVELS);
	return steals_at_level[level];
}

#ifdef WS_PAPI_PROFILE
#define mem_cache_misses(th) rdtsc()
#else
#define mem_cache_misses(th) 0
#endif
#endif /* IN_GCC */

#endif
