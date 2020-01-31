#ifndef CONFIG_H
#define CONFIG_H

 /*********************** OpenStream Runtime Configuration ***********************/

/*
 * The stack size of the workers.
 */

#define WSTREAM_STACK_SIZE 1 << 16

/*
 * The log2 of the initial size of the work queue, i.e., each worker starts with a 2^(WSTREAM_DF_DEQUE_LOG_SIZE) empty queue.
 */

#define WSTREAM_DF_DEQUE_LOG_SIZE 8

/*
 * When a worker did not successfully steal some work from any other workers,
 * the worker will relinquish the CPU and the thread is placed at the end of
 * the scheduler queue.
 * With this option active, workers will actively poll for new task without
 * relinquishing the CPU.
 */

#define WS_NO_YIELD_SPIN 0

/*
 * Disable workers local cache. A Task placed into this local cache cannot be
 * stolen by other workers. Static control program may temporarily stall until
 * the control program task finishes because of that.
 */

#define DISABLE_WQUEUE_LOCAL_CACHE 0

/*
 * If the producer burst is equal to the consumer window, defer the allocation
 * until the producer is scheduled for execution. If the producer burst is
 * smaller to the consumer window, the allocation is not deferred. The same
 * happens for views allocation, do not allocate until the producer is
 * scheduled for execution.
 */

#define DEFERRED_ALLOC 1

/*
 * When a worker is seeking a new task to execute, if no task is available in
 * the worker local cache or work queue, try stealing from a worker from which
 * a previous steal succeeded before visiting the numa-aware machine topology
 * tree to steal a task.
 */

#define CACHE_LAST_STEAL_VICTIM 1

/*
 * In openStream, a broadcast (peek operation) creates a copy of the viewed
 * data for each peeking task. With a broadcast table, the number of copy is
 * limited to one per numa node. This can significantly reduce the number of
 * copies but comes with extra runtime overhead of managing the table.
 * 
 * WARNING: Enabling broadcast tables requires rebuilding the source codes
 * using OpenStream.
 */

#define USE_BROADCAST_TABLES 0

/*
 * How should the workers be places when OMP_NUM_THREADS specifies a lower
 * amount of worker than what is available on the system. You can select
 * between the following options:
 * 
 * - distribute_minimise_worker_communication
 *      The workers are placed on processing units to minimise the communication
 *      latency between all the workers
 *
 * - distribute_minimise_worker_communication_hyperthreading_last
 *      The workers are placed on processing units to minimise the communication
 *      latency between all the workers but selects a hyperthreaded processing unit
 *      as a last resort
 * 
 * - distribute_maximise_per_worker_resources
 *      The workers are placed on processing units to maximise the resources
 *      available to each worker (cache and memory)
 */

#define WORKER_DISTRIBUTION_ALGORITHM distribute_minimise_worker_communication_hyperthreading_last

/*
 * Task pushing
 */

#define NUM_PUSH_SLOTS 0

/*
 * When pushing work to another worker, only allow to push to workers that are
 * not too close. PUSH_MIN_MEM_LEVEL represents the level in the HWLOC
 * hierarchy when such push is allowed.
 */

#define PUSH_MIN_MEM_LEVEL 1

/*
 * ??
 */

#define NUM_PUSH_REORDER_SLOTS 0

/*
 * ??
 */

#define PUSH_MIN_FRAME_SIZE (64 * 1024)
#define PUSH_MIN_REL_FRAME_SIZE 1.3

/*
 * Push to numa node in the order of numa nodes (SEQ) or randomly.
 */

#define PUSH_EQUAL_SEQ
// #define PUSH_EQUAL_RANDOM

/*
 * ??
 */

#define PUSH_STRATEGY_MAX_WRITER
//#define PUSH_STRATEGY_OWNER
//#define PUSH_STRATEGY_SPLIT_OWNER
//#define PUSH_STRATEGY_SPLIT_OWNER_CHAIN
//#define PUSH_STRATEGY_SPLIT_OWNER_CHAIN_INNER_MW
//#define PUSH_STRATEGY_SPLIT_SCORE_NODES

 /*********************** OpenStream Debug Options ***********************/

/*
 * Make the slab allocator more verbose by printing wasted memory and numa
 * mapping warnings.
 * This may have a performance impact.
 */

#define SLAB_ALLOCATOR_VERBOSE 0

/*
 * Make HWLOC print information about the hardware and worker placement. This
 * has no performance impact on the program other than the initialisation in
 * the pre_main function.
 */

#define HWLOC_VERBOSE 0

/*
 * Print the matrix representing the latency between two processing units. This
 * information should be provided by the operating system.
 */

#define HWLOC_PRINT_DISTANCE_MATRICES 0

 /*********************** OpenStream Profiling Options ***********************/

/*
 * Profile the work queues.
 *
 * WARNING: Enabling queue profiling requires rebuilding the source codes
 * using OpenStream.
 */

#define WQUEUE_PROFILE 0

/*
 * MATRIX_PROFILE profiles the amount of information exchanged between the the
 * worker threads through the streams. The information is dumped inside the
 * specified file in a matrix form (line worker id to column worker id).
 * 
 * Option requirement: WQUEUE_PROFILE
 */

#define MATRIX_PROFILE 0
#define MATRIX_PROFILE_OUTPUT "wqueue_matrix.out"

/*
 * Use linux getrusage function to gather resource usage for running threads.
 * This includes:
 *   - User CPU time
 *   - Major page faults (page to be allocated to the process)
 *   - Minor page faults (page already in memory but not mapped into the process)
 *   - The maximum resident set size (peak RAM usage of the process)
 *   - The number of involuntary context switches (e.g. kernel scheduler intervention)
 */

#define PROFILE_RUSAGE 0
 
 /*********************** OpenStream Tracing Aftermath ***********************/

/*
 * Enable a per-worker event sampling queue which is capable to store up to
 * MAX_WQEVENT_SAMPLES events. The trace will be stored in the file defined by
 * WQEVENT_SAMPLING_OUTFILE.
 */

#define MAX_WQEVENT_SAMPLES 0
#define WQEVENT_SAMPLING_OUTFILE "events.ost"

/*
 * The worker initialization state is registered into the trace. Otherwise, the
 * first state will be a work seeking state.
 */

#define TRACE_RT_INIT_STATE

/*
 * What on the silicon is this?
 */

#define MAX_WQEVENT_PARAVER_CYCLES -1

 /*********************** OpenStream Probably Broken Options (Post-HWLOC untested) ***********************/

//#define WS_PAPI_PROFILE
//#define WS_PAPI_MULTIPLEX

//#ifdef WS_PAPI_PROFILE
//#define WS_PAPI_NUM_EVENTS 2
//#define WS_PAPI_EVENTS { "PAPI_L1_DCM", "PAPI_L2_DCM" }
//#define MEM_CACHE_MISS_POS 0 /* Use L1_DCM as cache miss indicator */

// /* #define TRACE_PAPI_COUNTERS */
//#endif

/*********************** You Shall Not Touch ***********************/

#define ALLOW_WQEVENT_SAMPLING (MAX_WQEVENT_SAMPLES > 0)

#define ALLOW_PUSHES (NUM_PUSH_SLOTS > 0)

#define ALLOW_PUSH_REORDER (ALLOW_PUSHES && NUM_PUSH_REORDER_SLOTS > 0)

#ifndef IN_GCC
#include <string.h>

#ifdef WS_PAPI_PROFILE
#define mem_cache_misses(th) ((th)->papi_counters[MEM_CACHE_MISS_POS])
#else // !defined(WS_PAPI_PROFILE)
#define mem_cache_misses(th) 0
#endif // !defined(WS_PAPI_PROFILE)

#endif /* IN_GCC */

/*
 * Some configuration checks
 */
#if ALLOW_WQEVENT_SAMPLING && !WQUEUE_PROFILE
#error "ALLOW_WQEVENT_SAMPLING defined, but WQUEUE_PROFILE != 1"
#endif

#if MATRIX_PROFILE && !WQUEUE_PROFILE
#error "MATRIX_PROFILE defined, but WQUEUE_PROFILE != 1"
#endif

#if ALLOW_PUSHES && !WQUEUE_PROFILE
#error "WORK_PUSHING defined, but WQUEUE_PROFILE != 1"
#endif

#endif
