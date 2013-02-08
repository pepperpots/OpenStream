#ifndef CONFIG_H
#define CONFIG_H

#include <assert.h>

//#define _WSTREAM_DF_DEBUG 1
//#define _PAPI_PROFILE
//#define _PRINT_STATS

#define WSTREAM_DF_DEQUE_LOG_SIZE 8
#define MAX_NUM_CORES 1024
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

#define WQEVENT_SAMPLING_OUTFILE "events.prv"
#define WQEVENT_SAMPLING_PARFILE "parallelism.gpdata"

/* Description of the memory hierarchy */

#define MEM_NUM_LEVELS 5

/* 5 levels are defined:
 * 0: 1st level cache, private
 * 1: 2nd level cache, shared by 2 cores
 * 2: 3rd level cache, shared by 8 cores
 * 3: NUMA domain containing 8 cores
 * 4: whole machine, 64 cores in total
 */
static int mem_cores_at_level(unsigned int level)
{
	int cores_at_level[] = {1, 2, 8, 8, 64};
	assert(level < MEM_NUM_LEVELS);
	return cores_at_level[level];
}

/* Returns the name of a level in the memory hierarchy */
static const char* mem_level_name(unsigned int level)
{
	const char* level_names[] = {"same_l1",
				"same_l2",
				"same_l3",
				"same_numa",
				"same_machine"};
	assert(level < MEM_NUM_LEVELS);
	return level_names[level];
}

/* Checks if two cores are siblings at a level */
static int mem_level_siblings(unsigned int level, unsigned int a, unsigned int b)
{
	return (a / mem_cores_at_level(level)) ==
		(b / mem_cores_at_level(level));
}

/* Returns the lowest common level in the memory hierarchy between
 * two cores. For example, if a and b share the same L2 cache, then
 * the function returns the number of the level for L2 caches.
 */
static int mem_lowest_common_level(unsigned int a, unsigned int b)
{
	int level;

	for(level = 0; level < MEM_NUM_LEVELS; level++)
		if(mem_level_siblings(level, a, b))
			return level;

	assert(0);
}

/* Returns the n-th sibling of a core at a given level of the memory hierarchy.
 * The core ID returned is guaranteed to be different from the core passed
 * as an argument.
 */
static int mem_nth_sibling_at_level(unsigned int level, unsigned int cpu, unsigned int sibling_num)
{
	unsigned int base = cpu - (cpu % mem_cores_at_level(level));
	unsigned int sibling = base + sibling_num;

	if(sibling == cpu)
		return base + (((sibling_num + 1) % mem_cores_at_level(level)));

	return sibling;
}

/* Returns how many steal attempts at a given level should be performed. */
static int mem_num_steal_attempts_at_level(unsigned int level)
{
	int steals_at_level[] = {0, 2, 8, 1, 1};
	assert(level < MEM_NUM_LEVELS);
	return steals_at_level[level];
}
#endif
