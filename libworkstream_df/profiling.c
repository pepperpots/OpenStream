#include "profiling.h"
#include <string.h>
#include <stdio.h>
#include "wstream_df.h"
#include "numa.h"
#include <pthread.h>

#ifdef MATRIX_PROFILE
unsigned long long transfer_matrix[MAX_CPUS][MAX_CPUS];
static pthread_spinlock_t papi_spin_lock;

void init_transfer_matrix(void)
{
	memset(transfer_matrix, 0, sizeof(transfer_matrix));
}

void dump_transfer_matrix(unsigned int num_workers)
{
	unsigned int i, j;
	FILE* matrix_fp = fopen(MATRIX_PROFILE, "w+");
	assert(matrix_fp);

	for (i = 0; i < num_workers; ++i) {
		for (j = 0; j < num_workers; ++j) {
			fprintf(matrix_fp, "10%lld ", transfer_matrix[i][j]);
		}
		fprintf(matrix_fp, "\n");
	}
	fclose(matrix_fp);
}
#endif

#ifdef WS_PAPI_PROFILE

void
setup_papi(void)
{
	int retval;

	retval = PAPI_library_init(PAPI_VER_CURRENT);

	if (retval != PAPI_VER_CURRENT && retval > 0) {
		fprintf(stderr,"PAPI library version mismatch!\n");
		exit(1);
	}

	if (retval < 0) {
		fprintf(stderr,"Could not init PAPI library: %s (%d) %d!\n", PAPI_strerror(retval), retval, PAPI_VER_CURRENT);
		exit(1);
	}

	if (PAPI_is_initialized() != PAPI_LOW_LEVEL_INITED) {
		fprintf(stderr, "Could not init PAPI library (low-level part)!\n");
		exit(1);
	}

#if !defined(WS_PAPI_UNCORE) || WS_PAPI_UNCORE == 0
	if ((retval = PAPI_thread_init((unsigned long (*)(void)) (wstream_self))) != PAPI_OK) {
		fprintf(stderr, "Could not init threads: %s\n", PAPI_strerror(retval));
		exit(1);
	}
#endif

#ifdef WS_PAPI_MULTIPLEX
	if (PAPI_multiplex_init() != PAPI_OK) {
		fprintf(stderr, "Could not init multiplexing!\n");
		exit(1);
	}
#endif

#if defined(WS_PAPI_UNCORE) && WS_PAPI_UNCORE != 0
	if(pthread_spin_init(&papi_spin_lock, PTHREAD_PROCESS_PRIVATE) != 0) {
		fprintf(stderr, "Could not init papi spinlock!\n");
		exit(1);
	}
#endif
}

void
init_papi (wstream_df_thread_p th)
{
	int err, i;
	char* events[] = WS_PAPI_EVENTS;
	int component_idx = 0;

	if(sizeof(events) / sizeof(events[0]) > WS_PAPI_NUM_EVENTS) {
		fprintf(stderr, "Mismatch between WS_PAPI_NUM_EVENTS and the number of elements in WS_PAPI_EVENTS\n");
		exit(1);
	}

	memset(th->papi_counters, 0, sizeof(th->papi_counters));
	th->papi_count = 0;

#if !defined(WS_PAPI_UNCORE) || WS_PAPI_UNCORE == 0
	PAPI_register_thread();
#else
	pthread_spin_lock(&papi_spin_lock);
#endif

	th->papi_event_set = PAPI_NULL;

	/* Create event set */
	if ((err = PAPI_create_eventset(&th->papi_event_set)) != PAPI_OK) {
		fprintf(stderr, "Could not create event set for thread %d: %s!\n", th->worker_id, PAPI_strerror(err));
		exit(1);
	}

#if !defined(WS_PAPI_UNCORE) || WS_PAPI_UNCORE == 0
	component_idx = 0;
#else
	PAPI_get_component_index(WS_PAPI_UNCORE_COMPONENT);
#endif

	/* Assign CPU component */
	if ((err = PAPI_assign_eventset_component(th->papi_event_set, component_idx)) != PAPI_OK) {
		fprintf(stderr, "Could not assign event set to component 0 for thread %d: %s\n", th->worker_id, PAPI_strerror(err));
		exit(1);
	}

#ifdef WS_PAPI_MULTIPLEX
	/* Enable multiplexing */
	if((err = PAPI_set_multiplex(th->papi_event_set)) != PAPI_OK) {
		fprintf(stderr, "Could not enable multiplexing for event set: %s!\n", PAPI_strerror(err));
		exit(1);
	}
#endif

#if !defined(WS_PAPI_UNCORE) || WS_PAPI_UNCORE == 0
	th->papi_num_events = WS_PAPI_NUM_EVENTS;

	for(i = 0; i < WS_PAPI_NUM_EVENTS; i++) {
		th->papi_event_mapping[i] = i;

		if((err = PAPI_add_named_event(th->papi_event_set, events[i])) != PAPI_OK) {
			fprintf(stderr, "Could not add event \"%s\" to event set: %s!\n", events[i], PAPI_strerror(err));
			exit(1);
		}
	}
#else
	uint64_t uncore_masks[] = WS_PAPI_UNCORE_MASK;
	th->papi_num_events = 0;

	PAPI_cpu_option_t cpu_opt;
	cpu_opt.eventset = th->papi_event_set;
	cpu_opt.cpu_num = th->cpu;

	if((err = PAPI_set_opt(PAPI_CPU_ATTACH,(PAPI_option_t*)&cpu_opt)) != PAPI_OK) {
		fprintf(stderr, "Could not attach eventset to CPU: %s!\n", PAPI_strerror(err));
		exit(1);
	}

	/* we need to set the granularity to system-wide for uncore to work */
	PAPI_granularity_option_t gran_opt;
	gran_opt.def_cidx = 0;
	gran_opt.eventset = th->papi_event_set;
	gran_opt.granularity = PAPI_GRN_SYS;

	if((err = PAPI_set_opt(PAPI_GRANUL,(PAPI_option_t*)&gran_opt)) != PAPI_OK) {
		fprintf(stderr, "Could not set granularity: %s!\n", PAPI_strerror(err));
		exit(1);
	}

	/* we need to set domain to be as inclusive as possible */
	PAPI_domain_option_t domain_opt;
	domain_opt.def_cidx = 0;
	domain_opt.eventset = th->papi_event_set;
	domain_opt.domain = PAPI_DOM_ALL;

	if((err = PAPI_set_opt(PAPI_DOMAIN,(PAPI_option_t*)&domain_opt))) {
		fprintf(stderr, "Could not set domain: %s!\n", PAPI_strerror(err));
		exit(1);
	}

	for(int i = 0; i < WS_PAPI_NUM_EVENTS; i++) {
		if(uncore_masks[i] & ((uint64_t)1 << th->cpu)) {
			th->papi_num_events++;
			th->papi_event_mapping[th->papi_num_events-1] = i;

			if((err = PAPI_add_named_event(th->papi_event_set, events[i])) != PAPI_OK) {
				fprintf(stderr, "Could not add event \"%s\" to event set: %s!\n", events[i], PAPI_strerror(err));
				exit(1);
			}
		}
	}
#endif

#if defined(WS_PAPI_UNCORE) && WS_PAPI_UNCORE != 0
	pthread_spin_unlock(&papi_spin_lock);
#endif
}

void
update_papi_timestamp(struct wstream_df_thread* th, int64_t timestamp)
{
	if(!th->papi_count)
		return;

	if(th->papi_num_events > 0) {
		if(PAPI_accum(th->papi_event_set, th->papi_counters) != PAPI_OK) {
			fprintf(stderr, "Could not read counters for thread %d\n", th->worker_id);
			exit(1);
		}

#if ALLOW_WQEVENT_SAMPLING && defined(TRACE_PAPI_COUNTERS)
		for(int i = 0; i < th->papi_num_events; i++)
			trace_counter_timestamp(th, th->papi_event_mapping[i]+PAPI_COUNTER_BASE, th->papi_counters[i], timestamp+i);
#endif
	}
}

void
update_papi(struct wstream_df_thread* th)
{
	update_papi_timestamp(th, rdtsc());
}
#endif

#if WQUEUE_PROFILE
void
setup_wqueue_counters (void)
{
	setup_papi();
}

void
stop_wqueue_counters (void)
{
}

void
wqueue_counters_enter_runtime(struct wstream_df_thread* th)
{
}

void
init_wqueue_counters (wstream_df_thread_p th)
{
	th->steals_owncached = 0;
	th->steals_ownqueue = 0;
	memset(th->steals_mem, 0, sizeof(th->steals_mem));

	th->steals_fails = 0;
	th->tasks_created = 0;
	th->tasks_executed = 0;
	th->tasks_executed_localalloc = 0;
	th->tasks_executed_max_initial_writer = 0;

	memset(th->bytes_mem, 0, sizeof(th->bytes_mem));

#if ALLOW_PUSHES
	th->steals_pushed = 0;
	th->pushes_fails = 0;
	memset(th->pushes_mem, 0, sizeof(th->pushes_mem));
#endif

	th->reuse_addr = 0;
	th->reuse_copy = 0;

	init_papi(th);
}

void
dump_wqueue_counters_single (wstream_df_thread_p th)
{
	int level;

#ifdef WS_PAPI_PROFILE
	int i;
	const char* events[] = WS_PAPI_EVENTS;
#endif

	printf ("Thread %d: tasks_created = %lld\n",
		th->worker_id,
		th->tasks_created);
	printf ("Thread %d: tasks_executed = %lld\n",
		th->worker_id,
		th->tasks_executed);
	printf ("Thread %d: tasks_executed_localalloc = %lld\n",
		th->worker_id,
		th->tasks_executed_localalloc);
	printf ("Thread %d: tasks_executed_max_initial_writer = %lld\n",
		th->worker_id,
		th->tasks_executed_max_initial_writer);
	printf ("Thread %d: steals_owncached = %lld\n",
		th->worker_id,
		th->steals_owncached);
	printf ("Thread %d: steals_ownqueue = %lld\n",
		th->worker_id,
		th->steals_ownqueue);

	for(level = 0; level < MEM_NUM_LEVELS; level++) {
		printf ("Thread %d: steals_%s = %lld\n",
			th->worker_id,
			mem_level_name(level),
			th->steals_mem[level]);
	}

	printf ("Thread %d: steals_fails = %lld\n",
		th->worker_id,
		th->steals_fails);

#if ALLOW_PUSHES
	printf ("Thread %d: pushes_fails = %lld\n",
		th->worker_id,
		th->pushes_fails);
	printf ("Thread %d: steals_pushed = %lld\n",
		th->worker_id,
		th->steals_pushed);

	for(level = 0; level < MEM_NUM_LEVELS; level++) {
		printf ("Thread %d: pushes_%s = %lld\n",
			th->worker_id,
			mem_level_name(level),
			th->pushes_mem[level]);
	}
#endif

#ifdef WS_PAPI_PROFILE
	for(i = 0; i < WS_PAPI_NUM_EVENTS; i++) {
		printf ("Thread %d: papi_%s = %lld\n",
			th->worker_id,
			events[i],
			th->papi_counters[i]);
	}
#endif

	for(level = 0; level < MEM_NUM_LEVELS; level++) {
		printf ("Thread %d: bytes_%s = %lld\n",
		th->worker_id,
		mem_level_name(level),
		th->bytes_mem[level]);
	}
}

void dump_numa_counters_single(wstream_df_numa_node_p numa_node)
{
	printf ("Node %d: node_allocated_frame_bytes = %lld\n",
		numa_node->id,
		numa_node->frame_bytes_allocated);
	printf ("Node %d: slab_bytes = %lld\n",
		numa_node->id,
		numa_node->slab_cache.slab_bytes);
	printf ("Node %d: slab_final_objects = %d\n",
		numa_node->id,
		numa_node->slab_cache.num_objects);
	printf ("Node %d: slab_refills = %lld\n",
		numa_node->id,
		numa_node->slab_cache.slab_refills);
	printf ("Node %d: slab_allocations = %lld\n",
		numa_node->id,
		numa_node->slab_cache.slab_allocations);
	printf ("Node %d: slab_frees = %lld\n",
		numa_node->id,
		numa_node->slab_cache.slab_frees);
	printf ("Node %d: slab_freed_bytes = %lld\n",
		numa_node->id,
		numa_node->slab_cache.slab_freed_bytes);
	printf ("Node %d: slab_hits = %lld\n",
		numa_node->id,
		numa_node->slab_cache.slab_hits);
	printf ("Node %d: slab_toobig = %lld\n",
		numa_node->id,
		numa_node->slab_cache.slab_toobig);
	printf ("Node %d: slab_toobig_frees = %lld\n",
		numa_node->id,
		numa_node->slab_cache.slab_toobig_frees);
	printf ("Node %d: slab_toobig_freed_bytes = %lld\n",
		numa_node->id,
		numa_node->slab_cache.slab_toobig_freed_bytes);
}

void dump_wqueue_counters (unsigned int num_workers, wstream_df_thread_p* wstream_df_worker_threads)
{
	unsigned int i, level;
	unsigned long long bytes_mem[MEM_NUM_LEVELS];
	unsigned long long bytes_total = 0;

#ifdef WS_PAPI_PROFILE
	const char* events[] = WS_PAPI_EVENTS;
	long long papi_counters_accum[WS_PAPI_NUM_EVENTS];
	int evt;
#endif

	memset(bytes_mem, 0, sizeof(bytes_mem));

	for (i = 0; i < num_workers; ++i) {
		dump_wqueue_counters_single(&wstream_df_worker_threads[i]);

		for(level = 0; level < MEM_NUM_LEVELS; level++)
			bytes_mem[level] += wstream_df_worker_threads[i]->bytes_mem[level];
	}

	for (i = 0; i < MAX_NUMA_NODES; ++i) {
		dump_numa_counters_single(numa_node_by_id(i));
	}

	for(level = 0; level < MEM_NUM_LEVELS; level++)
		bytes_total += bytes_mem[level];

	for(level = 0; level < MEM_NUM_LEVELS; level++) {
		printf ("Overall bytes_%s = %lld (%f %%)\n",
			mem_level_name(level),
			bytes_mem[level],
			100.0*(double)bytes_mem[level]/(double)bytes_total);
	}

#ifdef WS_PAPI_PROFILE
	memset(papi_counters_accum, 0, sizeof(papi_counters_accum));

	for (i = 0; i < num_workers; ++i) {
		for(evt = 0; evt < WS_PAPI_NUM_EVENTS; evt++) {
			papi_counters_accum[evt] += wstream_df_worker_threads[i]->papi_counters[evt];
		}
	}

	for(evt = 0; evt < WS_PAPI_NUM_EVENTS; evt++) {
		printf ("Overall papi_%s = %lld\n",
			events[evt],
			papi_counters_accum[evt]);
	}
#endif
}

#endif
