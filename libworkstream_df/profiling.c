#include "profiling.h"
#include <string.h>
#include <stdio.h>
#include "wstream_df.h"

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

#ifdef WQUEUE_PROFILE
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
}

void
dump_wqueue_counters_single (wstream_df_thread_p th)
{
	int level;

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
}

#endif
