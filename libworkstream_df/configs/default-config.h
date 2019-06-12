#ifndef CONFIG_H
#define CONFIG_H

#include <assert.h>
#include "../arch.h"

//#define _WSTREAM_DF_DEBUG 1
//#define _PAPI_PROFILE
//#define _PRINT_STATS
//#define _WS_NO_YIELD_SPIN

#define WSTREAM_DF_DEQUE_LOG_SIZE 8
#define WSTREAM_STACK_SIZE 1 << 16

#define MAX_CPUS 64
#define MAX_NUMA_NODES 1
#define UNIFORM_MEMORY_ACCESS

#define WQUEUE_PROFILE 0
#define MATRIX_PROFILE "wqueue_matrix.out"

#define PUSH_MIN_MEM_LEVEL 1
#define PUSH_MIN_FRAME_SIZE (64*1024)
#define PUSH_MIN_REL_FRAME_SIZE 1.3
#define NUM_PUSH_SLOTS 0
#define ALLOW_PUSHES (NUM_PUSH_SLOTS > 0)

#define NUM_PUSH_REORDER_SLOTS 0
#define ALLOW_PUSH_REORDER (ALLOW_PUSHES && NUM_PUSH_REORDER_SLOTS > 0)

#define DEFERRED_ALLOC

#define CACHE_LAST_STEAL_VICTIM 0

#define MAX_WQEVENT_SAMPLES 0
#define TRACE_RT_INIT_STATE
/* #define TRACE_DATA_READS */
#define ALLOW_WQEVENT_SAMPLING (MAX_WQEVENT_SAMPLES > 0)
#define MAX_WQEVENT_PARAVER_CYCLES -1
#define NUM_WQEVENT_TASKHIST_BINS 1000

#define WQEVENT_SAMPLING_OUTFILE "events.ost"
#define WQEVENT_SAMPLING_PARFILE "parallelism.gpdata"
#define WQEVENT_SAMPLING_TASKHISTFILE "task_histogram.gpdata"
#define WQEVENT_SAMPLING_TASKLENGTHFILE "task_length.gpdata"

#ifndef LOG_MPI_ACTIVITY
#  define LOG_MPI_ACTIVITY 0
#endif
#if LOG_MPI_ACTIVITY
#  define LOG_MPI(...) wstream_df_log(__VA_ARGS__)
#else
#  define LOG_MPI(...)
#endif

//#define WS_PAPI_PROFILE
//#define WS_PAPI_MULTIPLEX

//#ifdef WS_PAPI_PROFILE
//#define WS_PAPI_NUM_EVENTS 2
//#define WS_PAPI_EVENTS { "PAPI_L1_DCM", "PAPI_L2_DCM" }
//#define MEM_CACHE_MISS_POS 0 /* Use L1_DCM as cache miss indicator */

// /* #define TRACE_PAPI_COUNTERS */
//#endif

/* Description of the memory hierarchy */
#define MEM_NUM_LEVELS 2

#ifndef IN_GCC
#include <string.h>

/* Unknown memory hierarchy, 2 levels are defined:
 * 0: The core itself, probably with a private 1st level cache
 * 1: All of the machine's cores
 */
static inline int mem_cores_at_level(unsigned int level, unsigned int cpu)
{
	assert(level < MEM_NUM_LEVELS);

	if(level == 0)
		return 1;

	return wstream_df_num_cores_cached();
}

/* Returns the name of a level in the memory hierarchy */
static inline const char* mem_level_name(unsigned int level)
{
	const char* level_names[] = {"core", "machine"};
	assert(level < MEM_NUM_LEVELS);
	return level_names[level];
}

/* Checks if two cores are siblings at a level */
static inline int mem_level_siblings(unsigned int level, unsigned int a, unsigned int b)
{
	if(level == 0)
		return a == b;

	return 1;
}

/* Returns the lowest common level in the memory hierarchy between
 * two cores. For example, if a and b share the same L2 cache, then
 * the function returns the number of the level for L2 caches.
 */
static inline int mem_lowest_common_level(unsigned int a, unsigned int b)
{
	if(a == b)
		return 0;

	return 1;
}

static inline int mem_transfer_costs(unsigned int a, unsigned int b)
{
	static int level_transfer_costs[MEM_NUM_LEVELS] = {0, 1};

	int common_level = mem_lowest_common_level(a, b);
	return level_transfer_costs[common_level];
}

/* Returns the n-th sibling of a core at a given level of the memory hierarchy.
 * The core ID returned is guaranteed to be different from the core passed
 * as an argument.
 */
static inline int mem_nth_sibling_at_level(unsigned int level, unsigned int cpu, unsigned int sibling_num)
{
	unsigned int base = cpu - (cpu % mem_cores_at_level(level, cpu));
	unsigned int sibling = base + sibling_num;

	if(sibling == cpu)
		return base + (((sibling_num + 1) % mem_cores_at_level(level, cpu)));

	return sibling;
}


/* Returns how many steal attempts at a given level should be performed. */
static inline int mem_num_steal_attempts_at_level(unsigned int level)
{
	int steals_at_level[] = {0, 1};
	assert(level < MEM_NUM_LEVELS);
	return steals_at_level[level];
}


static inline void mem_estimate_frame_transfer_costs(int metadata_owner,
						     int* bytes_cpu,
						     long long* cache_misses,
						     long long* cache_misses_now,
						     int allocator,
						     unsigned long long* costs)
{
	int i, j;
	unsigned long long cost;
	unsigned long long bytes;

	memset(costs, 0, sizeof(*costs)*MAX_CPUS);

	for(i = 0; i < MAX_CPUS; i++) {
		for(j = 0; j < MAX_CPUS; j++) {
			bytes = bytes_cpu[i];

			if(i == metadata_owner)
				bytes += 1024;

			if(bytes) {
				cost = mem_transfer_costs(i, j);
				costs[j] += cost * bytes;
			}
		}
	}
}

static inline unsigned int mem_numa_node(unsigned int cpu)
{
	return 0;
}

#ifdef WS_PAPI_PROFILE
#define mem_cache_misses(th) ((th)->papi_counters[MEM_CACHE_MISS_POS])
#else
#define mem_cache_misses(th) 0
#endif
#endif /* IN_GCC */

#endif

