#ifndef CONFIG_H
#define CONFIG_H

#include <assert.h>

//#define _WSTREAM_DF_DEBUG 1
//#define _PAPI_PROFILE
//#define _PRINT_STATS
//#define _WS_NO_YIELD_SPIN

#define WSTREAM_DF_DEQUE_LOG_SIZE 8
#define WSTREAM_STACK_SIZE 1 << 16

#define MAX_CPUS 24

#define WQUEUE_PROFILE 1
#define MATRIX_PROFILE "wqueue_matrix.out"

#define PUSH_MIN_MEM_LEVEL 3
#define PUSH_MIN_FRAME_SIZE (64*1024)
#define PUSH_MIN_REL_FRAME_SIZE 1.3
#define NUM_PUSH_SLOTS 32
#define ALLOW_PUSHES (NUM_PUSH_SLOTS > 0)

#define NUM_PUSH_REORDER_SLOTS 0
#define ALLOW_PUSH_REORDER (ALLOW_PUSHES && NUM_PUSH_REORDER_SLOTS > 0)

#define CACHE_LAST_STEAL_VICTIM 0

#define MAX_WQEVENT_SAMPLES 10000000
#define TRACE_RT_INIT_STATE
#define ALLOW_WQEVENT_SAMPLING (MAX_WQEVENT_SAMPLES > 0)
#define MAX_WQEVENT_PARAVER_CYCLES -1
#define NUM_WQEVENT_TASKHIST_BINS 1000

#define WQEVENT_SAMPLING_OUTFILE "events.ost"
#define WQEVENT_SAMPLING_PARFILE "parallelism.gpdata"
#define WQEVENT_SAMPLING_TASKHISTFILE "task_histogram.gpdata"
#define WQEVENT_SAMPLING_TASKLENGTHFILE "task_length.gpdata"

#define WS_PAPI_PROFILE
#define WS_PAPI_MULTIPLEX

#ifdef WS_PAPI_PROFILE
#define WS_PAPI_NUM_EVENTS 2
#define WS_PAPI_EVENTS { PAPI_L1_DCM, PAPI_L2_DCM }
#define MEM_CACHE_MISS_POS 0 /* Use L1_DCM as cache miss indicator */
#endif

/* Description of the memory hierarchy */
#define MEM_NUM_LEVELS 5
#define MEM_CACHE_LINE_SIZE 64

#ifndef IN_GCC
#include <string.h>

/* 5 levels are defined:
 * 0: 1st level cache, private
 * 1: 2nd level cache, private
 * 2: 3rd level cache, shared by 6 cores
 * 3: 12 Cores in the same NUMA domain
 * 4: 24 Cores for the whole machine
 */
static inline int mem_cores_at_level(unsigned int level)
{
	int cores_at_level[] = {1, 1, 6, 12, 24};
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
				"machine"};
	assert(level < MEM_NUM_LEVELS);
	return level_names[level];
}

/* Checks if two cores are siblings at a level */
static inline int mem_level_siblings(unsigned int level, unsigned int a, unsigned int b)
{
	switch(level) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
			return (a / mem_cores_at_level(level)) ==
				(b / mem_cores_at_level(level));
		case 5: return 1;
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
		0, 3, 10, 100, 200
	};

	int common_level = mem_lowest_common_level(a, b);
	return level_transfer_costs[common_level];
}

/* Returns the n-th sibling of a core at a given level of the memory hierarchy.
 * The core ID returned is guaranteed to be different from the core passed
 * as an argument.
 */
static inline int mem_nth_sibling_at_level(unsigned int level, unsigned int cpu, unsigned int sibling_num)
{
	unsigned int base = cpu - (cpu % mem_cores_at_level(level));
	unsigned int sibling = base + sibling_num;

	if(sibling == cpu)
		return base + (((sibling_num + 1) % mem_cores_at_level(level)));

	return sibling;
}

/* Returns how many steal attempts at a given level should be performed. */
static inline int mem_num_steal_attempts_at_level(unsigned int level)
{
	int steals_at_level[] = {0, 0, 6, 1, 1};

	/* Configuration for complete random work-stealing */
	/* int steals_at_level[] = {0, 0, 0, 0, 1}; */

	assert(level < MEM_NUM_LEVELS);
	return steals_at_level[level];
}


static inline void mem_estimate_frame_transfer_costs(int metadata_owner, int* bytes_cpu, long long* cache_misses, long long* cache_misses_now, int allocator, unsigned long long* costs)
{
	int i, j;
	unsigned long long cost;
	unsigned long long cost_allocator;
	unsigned long long bytes;
	unsigned long long bytes_at_allocator = 0;
	unsigned long long cache_miss_diff;

	memset(costs, 0, sizeof(*costs)*MAX_CPUS);

	for(i = 0; i < MAX_CPUS; i++) {
		for(j = 0; j < MAX_CPUS; j++) {
			bytes = bytes_cpu[i];

			if(i == metadata_owner)
				bytes += 1024;

			if(bytes) {
				cost = mem_transfer_costs(i, j);
				cost_allocator = mem_transfer_costs(j, allocator);

				cache_miss_diff = cache_misses_now[i] - cache_misses[i];

				if(cache_miss_diff*30 < bytes/MEM_CACHE_LINE_SIZE)
					bytes_at_allocator = cache_miss_diff*MEM_CACHE_LINE_SIZE/30;
				else
					bytes_at_allocator = 0;

				costs[j] += cost * (bytes-bytes_at_allocator);
				costs[j] += cost_allocator * bytes_at_allocator;
			}
		}
	}
}

#ifdef WS_PAPI_PROFILE
#define mem_cache_misses(th) ((th)->papi_counters[MEM_CACHE_MISS_POS])
#else
#define mem_cache_misses(th) 0
#endif
#endif /* IN_GCC */

#endif
