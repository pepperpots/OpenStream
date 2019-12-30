#include "profiling.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "wstream_df.h"
#include "numa.h"
#include <pthread.h>

#if PROFILE_RUSAGE
#include <sys/time.h>
#include <sys/resource.h>
#endif // PROFILE_RUSAGE

#if MATRIX_PROFILE
#include <assert.h>

void *tm_data__;

void init_transfer_matrix(void) {
  tm_data__ = calloc(
      1, sizeof(unsigned long long[wstream_num_workers][wstream_num_workers]));
  assert(tm_data__ != NULL);
}

void dump_transfer_matrix(unsigned int num_workers)
{
	unsigned int i, j;
	FILE* matrix_fp = fopen(MATRIX_PROFILE_OUTPUT, "w+");
	assert(matrix_fp);

	for (i = 0; i < num_workers; ++i) {
		for (j = 0; j < num_workers; ++j) {
			fprintf(matrix_fp, "10%lld ", transfer_matrix[i][j]);
		}
		fprintf(matrix_fp, "\n");
	}
	fclose(matrix_fp);
}

extern inline void inc_transfer_matrix_entry(unsigned int consumer,
                                             unsigned int producer,
                                             unsigned long long num_bytes);

#endif // MATRIX_PROFILE

#ifdef WS_PAPI_PROFILE

void load_papi_env(){

	const char* env_papi_events = getenv(WS_PAPI_EVENTS_ENV_VAR);
	if(env_papi_events == NULL){
		fprintf(stderr,"OpenStream PAPI profiling is enabled, but no events are being traced.\n");
		papi_num_events = 0;
		papi_multiplex_enable = 0;
		return;
	}

	if(env_papi_events[0] == '\0'){
		papi_num_events = 0;
		papi_multiplex_enable = 0;
	}

	// Copy the events from the environment as strtok mutates in place
	char papi_events_str[strlen(env_papi_events)];
	strcpy(&papi_events_str[0],env_papi_events);

	// Parse each event into the global config variable papi_event_names
	// User input events may be invalid, but deferring error to PAPI library later
	char* event;
	char* delimiter = ",";
	int event_index = -1;

	event = strtok(papi_events_str, delimiter);
	while(event != NULL){
		event_index++;

		if(event_index >= WS_PAPI_MAX_NUM_EVENTS){
			fprintf(stderr,"Number of PAPI events exceeded limit (%d).\n",WS_PAPI_MAX_NUM_EVENTS);
			exit(1);
		}
		
		papi_event_names[event_index] = malloc(strlen(event)*sizeof(char));
		
		if(papi_event_names[event_index] == NULL){
			fprintf(stdout,"Failed to allocate memory when parsing PAPI events from environment.\n");
			exit(1);
		}

		strcpy(papi_event_names[event_index],event);
		event = strtok(NULL, delimiter);
	}
	
	papi_num_events = event_index + 1;

	const char* env_papi_multiplex = getenv(WS_PAPI_MULTIPLEX_ENV_VAR);
	if(env_papi_multiplex == NULL || env_papi_multiplex[0] == '\0'){
		papi_multiplex_enable = WS_PAPI_MULTIPLEX;
	} else if(env_papi_multiplex[0] == '0'){
		papi_multiplex_enable = 0;
	} else if(env_papi_multiplex[0] == '1'){
		papi_multiplex_enable = 1;
	}

}

unsigned long long_wstream_handle() {
	return wstream_self();
}

void
setup_papi(void)
{
	int retval;

	load_papi_env();

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
	if ((retval = PAPI_thread_init(long_wstream_handle)) != PAPI_OK) {
		fprintf(stderr, "Could not init threads: %s\n", PAPI_strerror(retval));
		exit(1);
	}
#endif

	if(papi_multiplex_enable){
		if (PAPI_multiplex_init() != PAPI_OK) {
			fprintf(stderr, "Could not init multiplexing!\n");
			exit(1);
		}
	}

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
	int component_idx = 0;

	memset(th->papi_counters, 0, sizeof(th->papi_counters));
	th->papi_count = 0;
	th->papi_reset = 1;

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

	if(papi_multiplex_enable){
		/* Enable multiplexing */
		if((err = PAPI_set_multiplex(th->papi_event_set)) != PAPI_OK) {
			fprintf(stderr, "Could not enable multiplexing for event set: %s!\n", PAPI_strerror(err));
			exit(1);
		}
	}

#if !defined(WS_PAPI_UNCORE) || WS_PAPI_UNCORE == 0
	th->papi_num_events = papi_num_events;

	for(i = 0; i < papi_num_events; i++) {
		th->papi_event_mapping[i] = i;

		if((err = PAPI_add_named_event(th->papi_event_set, papi_event_names[i])) != PAPI_OK) {
			fprintf(stderr, "Could not add event \"%s\" to event set: %s!\n", papi_event_names[i], PAPI_strerror(err));
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

	for(int i = 0; i < papi_num_events; i++) {
		if(uncore_masks[i] & ((uint64_t)1 << th->cpu)) {
			th->papi_num_events++;
			th->papi_event_mapping[th->papi_num_events-1] = i;

			if((err = PAPI_add_named_event(th->papi_event_set, papi_event_names[i])) != PAPI_OK) {
				fprintf(stderr, "Could not add event \"%s\" to event set: %s!\n", papi_event_names[i], PAPI_strerror(err));
				exit(1);
			}
		}
	}
#endif

	if(papi_num_events > 0){
		if((err = PAPI_start(th->papi_event_set)) != PAPI_OK) {
			fprintf(stderr, "Worker %d could not start counters: %s!\n", th->worker_id, PAPI_strerror(err));
			exit(1);
		}   
		th->papi_count = 1;
	}

#if defined(WS_PAPI_UNCORE) && WS_PAPI_UNCORE != 0
	pthread_spin_unlock(&papi_spin_lock);
#endif
}

void
update_papi_timestamp(struct wstream_df_thread* th, int64_t timestamp)
{
	if(!th->papi_count)
		return;

	if(th->papi_reset){
		int err;
		if ((err = PAPI_reset(th->papi_event_set)) != PAPI_OK) {
			fprintf(stderr, "Could not reset counters: %s!\n", PAPI_strerror(err));
			exit(1);
		}
		th->papi_reset = 0;
	}

	if(th->papi_num_events > 0) {
		if(PAPI_accum(th->papi_event_set, th->papi_counters) != PAPI_OK) {
			fprintf(stderr, "Could not read counters for thread %d\n", th->worker_id);
			exit(1);
		}

#if ALLOW_WQEVENT_SAMPLING
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

#if PROFILE_RUSAGE

void wqueue_counters_profile_rusage(struct wstream_df_thread *th) {
  struct rusage usage;

  if (getrusage(RUSAGE_THREAD, &usage))
    wstream_df_fatal("Call to getrusage failed!");

  th->system_time_us = (unsigned long long)usage.ru_stime.tv_sec * 1000000 +
                       (unsigned long long)usage.ru_stime.tv_usec;
  th->major_page_faults = usage.ru_majflt;
  th->minor_page_faults = usage.ru_minflt;
  th->max_resident_size = usage.ru_maxrss;
  th->inv_context_switches = usage.ru_nivcsw;
}

#endif // PROFILE_RUSAGE

void init_wqueue_counters(wstream_df_thread_p th) {
  th->steals_owncached = 0;
  th->steals_ownqueue = 0;
  th->steals_mem = calloc(topology_depth, sizeof(*th->steals_mem));
  if (bind_memory_to_cpu_memspace(
          th->steals_mem, topology_depth * sizeof(*th->steals_mem), th->cpu)) {
#if HWLOC_VERBOSE
    perror("hwloc_membind");
#endif // HWLOC_VERBOSE
  }

  th->steals_fails = 0;
  th->tasks_created = 0;
  th->tasks_executed = 0;
  th->tasks_executed_localalloc = 0;

  th->bytes_mem = calloc(topology_depth, sizeof(*th->bytes_mem));
  if (bind_memory_to_cpu_memspace(
          th->bytes_mem, topology_depth * sizeof(*th->bytes_mem), th->cpu)) {
#if HWLOC_VERBOSE
    perror("hwloc_membind");
#endif // HWLOC_VERBOSE
  }

#if ALLOW_PUSHES
  th->steals_pushed = 0;
  th->pushes_fails = 0;
  th->pushes_mem = calloc(topology_depth, sizeof(*th->pushes_mem));
#endif

  th->reuse_addr = 0;
  th->reuse_copy = 0;
#if PROFILE_RUSAGE
  th->system_time_us = 0;
  th->major_page_faults = 0;
  th->minor_page_faults = 0;
  th->max_resident_size = 0;
  th->inv_context_switches = 0;
#endif // PROFILE_RUSAGE

  init_papi(th);
}

void
dump_wqueue_counters_single (wstream_df_thread_p th)
{

#ifdef WS_PAPI_PROFILE
	int i;
#endif // defined(WS_PAPI_PROFILE)

#if PROFILE_RUSAGE
	printf ("Thread %d: system_time_us = %lld\n",
		th->worker_id,
		th->system_time_us);
	printf ("Thread %d: major_page_faults = %lld\n",
		th->worker_id,
		th->major_page_faults);
	printf ("Thread %d: minor_page_faults = %lld\n",
		th->worker_id,
		th->minor_page_faults);
	printf ("Thread %d: max_resident_size = %lld\n",
		th->worker_id,
		th->max_resident_size);
	printf ("Thread %d: inv_context_switches = %lld\n",
		th->worker_id,
		th->inv_context_switches);
#endif // PROFILE_RUSAGE
	printf ("Thread %d: tasks_created = %lld\n",
		th->worker_id,
		th->tasks_created);
	printf ("Thread %d: tasks_executed = %lld\n",
		th->worker_id,
		th->tasks_executed);
	printf ("Thread %d: tasks_executed_localalloc = %lld\n",
		th->worker_id,
		th->tasks_executed_localalloc);
	printf ("Thread %d: steals_owncached = %lld\n",
		th->worker_id,
		th->steals_owncached);
	printf ("Thread %d: steals_ownqueue = %lld\n",
		th->worker_id,
		th->steals_ownqueue);

	for(unsigned level = 0; level < topology_depth; level++) {
		printf ("Thread %d: steals_%s = %lld\n",
			th->worker_id,
			level_name_hwloc(level),
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

	for(unsigned level = 0; level < topology_depth; level++) {
		printf ("Thread %d: pushes_%s = %lld\n",
			th->worker_id,
			level_name_hwloc(level),
			th->pushes_mem[level]);
	}
#endif

#ifdef WS_PAPI_PROFILE
	for(i = 0; i < th->papi_num_events; i++) {
		printf ("Thread %d: papi_%s = %lld\n",
			th->worker_id,
			papi_event_names[i],
			th->papi_counters[i]);
	}
#endif

	for(unsigned level = 0; level < topology_depth; level++) {
		printf ("Thread %d: bytes_%s = %lld\n",
		th->worker_id,
		level_name_hwloc(level),
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
#ifdef WS_PAPI_PROFILE
	#ifdef DUMP_NUMA_COUNTERS
	for(int i = 0; i < num_numa_nodes; i++) {
		dump_numa_counters_single(numa_node_by_id(i));
	}
	#endif
	
	int papi_num_events = wstream_df_worker_threads[0]->papi_num_events;
	long long papi_counters_accum[papi_num_events];
	int evt;

	memset(papi_counters_accum, 0, sizeof(papi_counters_accum));

	for (int i = 0; i < num_workers; ++i) {
		for(evt = 0; evt < papi_num_events; evt++) {
			papi_counters_accum[evt] += wstream_df_worker_threads[i]->papi_counters[evt];
		}
	}

	for(evt = 0; evt < papi_num_events; evt++) {
		printf ("Overall papi_%s = %lld\n",
			papi_event_names[evt],
			papi_counters_accum[evt]);
	}
#endif
}

#endif
