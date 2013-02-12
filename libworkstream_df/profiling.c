#include "profiling.h"
#include <string.h>
#include <stdio.h>
#include "wstream_df.h"
#include <pthread.h>

#ifdef MATRIX_PROFILE
unsigned long long transfer_matrix[MAX_CPUS][MAX_CPUS];

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

	if ((retval = PAPI_thread_init((unsigned long (*)(void)) (pthread_self))) != PAPI_OK) {
		fprintf(stderr, "Could not init threads: %s\n", PAPI_strerror(retval));
		exit(1);
	}

#ifdef WS_PAPI_MULTIPLEX
		if (PAPI_multiplex_init() != PAPI_OK) {
			fprintf(stderr, "Could not init multiplexing!\n");
			exit(1);
		}
#endif
}

void
init_papi (wstream_df_thread_p th)
{
	int err, i;
	char event_name[PAPI_MAX_STR_LEN];
	int event_codes[] = WS_PAPI_EVENTS;

	if(sizeof(event_codes) / sizeof(event_codes[0]) > WS_PAPI_NUM_EVENTS) {
		fprintf(stderr, "Mismatch between WS_PAPI_NUM_EVENTS and the number of elements in WS_PAPI_EVENTS\n");
		exit(1);
	}

	memset(th->papi_counters, 0, sizeof(th->papi_counters));

	PAPI_register_thread();
	th->papi_event_set = PAPI_NULL;

	/* Create event set */
	if ((err = PAPI_create_eventset(&th->papi_event_set)) != PAPI_OK) {
		fprintf(stderr, "Could not create event set for thread %d: %s!\n", th->worker_id, PAPI_strerror(err));
		exit(1);
	}

	/* Assign CPU component */
	if ((err = PAPI_assign_eventset_component(th->papi_event_set, 0)) != PAPI_OK) {
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

	for(i = 0; i < WS_PAPI_NUM_EVENTS; i++) {
		PAPI_event_code_to_name (event_codes[i], event_name);
		if((err = PAPI_add_event(th->papi_event_set, event_codes[i])) != PAPI_OK) {
			fprintf(stderr, "Could not add event Ox%X (\"%s\") to event set: %s!\n", event_codes[i], event_name, PAPI_strerror(err));
			exit(1);
		}
	}

	/* Start counting */
	if (PAPI_start(th->papi_event_set) != PAPI_OK) {
		fprintf(stderr, "Could not start counters!\n");
		exit(1);
	}
}

void
update_papi(struct wstream_df_thread* th)
{
	if(PAPI_read(th->papi_event_set, th->papi_counters) != PAPI_OK) {
		fprintf(stderr, "Could not read counters for thread %d\n", th->worker_id);
		exit(1);
	}
}
#endif

#ifdef WQUEUE_PROFILE
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
	update_papi(th);
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

#if ALLOW_PUSHES
	th->steals_pushed = 0;
	th->pushes_fails = 0;
	memset(th->pushes_mem, 0, sizeof(th->pushes_mem));
#endif

	init_papi(th);
}

void
dump_wqueue_counters_single (wstream_df_thread_p th)
{
	int level;

#ifdef WS_PAPI_PROFILE
	int i;
	char event_name[PAPI_MAX_STR_LEN];
	int event_codes[] = WS_PAPI_EVENTS;
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

#if !NO_SLAB_ALLOCATOR
	printf ("Thread %d: slab_bytes = %lld\n",
		th->worker_id,
		th->slab_cache.slab_bytes);
	printf ("Thread %d: slab_refills = %lld\n",
		th->worker_id,
		th->slab_cache.slab_refills);
	printf ("Thread %d: slab_allocations = %lld\n",
		th->worker_id,
		th->slab_cache.slab_allocations);
	printf ("Thread %d: slab_frees = %lld\n",
		th->worker_id,
		th->slab_cache.slab_frees);
	printf ("Thread %d: slab_freed_bytes = %lld\n",
		th->worker_id,
		th->slab_cache.slab_freed_bytes);
	printf ("Thread %d: slab_hits = %lld\n",
		th->worker_id,
		th->slab_cache.slab_hits);
	printf ("Thread %d: slab_toobig = %lld\n",
		th->worker_id,
		th->slab_cache.slab_toobig);
	printf ("Thread %d: slab_toobig_frees = %lld\n",
		th->worker_id,
		th->slab_cache.slab_toobig_frees);
	printf ("Thread %d: slab_toobig_freed_bytes = %lld\n",
		th->worker_id,
		th->slab_cache.slab_toobig_freed_bytes);
#endif

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
		PAPI_event_code_to_name (event_codes[i], event_name);

		printf ("Thread %d: papi_%s = %lld\n",
			th->worker_id,
			event_name,
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

void dump_wqueue_counters (unsigned int num_workers, wstream_df_thread_p wstream_df_worker_threads)
{
	unsigned int i, level;
	unsigned long long bytes_mem[MEM_NUM_LEVELS];
	unsigned long long bytes_total = 0;

#ifdef WS_PAPI_PROFILE
	char event_name[PAPI_MAX_STR_LEN];
	int event_codes[] = WS_PAPI_EVENTS;
	long long papi_counters_accum[WS_PAPI_NUM_EVENTS];
	int evt;
#endif

	memset(bytes_mem, 0, sizeof(bytes_mem));

	for (i = 0; i < num_workers; ++i) {
		dump_wqueue_counters_single(&wstream_df_worker_threads[i]);

		for(level = 0; level < MEM_NUM_LEVELS; level++)
			bytes_mem[level] += wstream_df_worker_threads[i].bytes_mem[level];
	}

	for(level = 0; level < MEM_NUM_LEVELS; level++)
		bytes_total += bytes_mem[level];

	for(level = 0; level < MEM_NUM_LEVELS; level++) {
		printf ("Oveall bytes_%s = %lld (%f %%)\n",
			mem_level_name(level),
			bytes_mem[level],
			100.0*(double)bytes_mem[level]/(double)bytes_total);
	}

#ifdef WS_PAPI_PROFILE
	memset(papi_counters_accum, 0, sizeof(papi_counters_accum[evt]));

	for (i = 0; i < num_workers; ++i) {
		for(evt = 0; evt < WS_PAPI_NUM_EVENTS; evt++) {
			papi_counters_accum[evt] += wstream_df_worker_threads[i].papi_counters[evt];
		}
	}

	for(evt = 0; evt < WS_PAPI_NUM_EVENTS; evt++) {
		PAPI_event_code_to_name (event_codes[evt], event_name);
		printf ("Overall papi_%s = %lld\n",
			event_name,
			papi_counters_accum[evt]);
	}
#endif
}

#endif
