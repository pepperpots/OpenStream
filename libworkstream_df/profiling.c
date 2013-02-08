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
	th->steals_samel2 = 0;
	th->steals_samel3 = 0;
	th->steals_remote = 0;
	th->steals_fails = 0;
	th->tasks_created = 0;
	th->tasks_executed = 0;

#ifdef ALLOW_PUSHES
	th->steals_pushed = 0;
	th->pushes_samel2 = 0;
	th->pushes_samel3 = 0;
	th->pushes_remote = 0;
	th->pushes_fails = 0;
#endif
}

void
dump_wqueue_counters_single (wstream_df_thread_p th)
{
	printf ("Thread %d: tasks_created = %lld\n",
		th->worker_id,
		th->tasks_created);
	printf ("Thread %d: tasks_executed = %lld\n",
		th->worker_id,
		th->tasks_executed);
	printf ("Thread %d: steals_owncached = %lld\n",
		th->worker_id,
		th->steals_owncached);
	printf ("Thread %d: steals_ownqueue = %lld\n",
		th->worker_id,
		th->steals_ownqueue);
	printf ("Thread %d: steals_samel2 = %lld\n",
		th->worker_id,
		th->steals_samel2);
	printf ("Thread %d: steals_samel3 = %lld\n",
		th->worker_id,
		th->steals_samel3);
	printf ("Thread %d: steals_remote = %lld\n",
		th->worker_id,
		th->steals_remote);
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

#ifdef ALLOW_PUSHES
	printf ("Thread %d: pushes_samel2 = %lld\n",
		th->worker_id,
		th->pushes_samel2);
	printf ("Thread %d: pushes_samel3 = %lld\n",
		th->worker_id,
		th->pushes_samel3);
	printf ("Thread %d: pushes_remote = %lld\n",
		th->worker_id,
		th->pushes_remote);
	printf ("Thread %d: pushes_fails = %lld\n",
		th->worker_id,
		th->pushes_fails);
	printf ("Thread %d: steals_pushed = %lld\n",
		th->worker_id,
		th->steals_pushed);
#endif

	printf ("Thread %d: bytes_l1 = %lld\n",
		th->worker_id,
		th->bytes_l1);
	printf ("Thread %d: bytes_l2 = %lld\n",
		th->worker_id,
		th->bytes_l2);
	printf ("Thread %d: bytes_l3 = %lld\n",
		th->worker_id,
		th->bytes_l3);
	printf ("Thread %d: bytes_rem = %lld\n",
		th->worker_id,
		th->bytes_rem);
}

void dump_wqueue_counters (unsigned int num_workers, wstream_df_thread_p wstream_df_worker_threads)
{
	unsigned int i;
	unsigned long long bytes_l1 = 0;
	unsigned long long bytes_l2 = 0;
	unsigned long long bytes_l3 = 0;
	unsigned long long bytes_rem = 0;
	unsigned long long bytes_total = 0;

	for (i = 0; i < num_workers; ++i) {
		dump_wqueue_counters_single(&wstream_df_worker_threads[i]);
		bytes_l1 += wstream_df_worker_threads[i].bytes_l1;
		bytes_l2 += wstream_df_worker_threads[i].bytes_l2;
		bytes_l3 += wstream_df_worker_threads[i].bytes_l3;
		bytes_rem += wstream_df_worker_threads[i].bytes_rem;
	}
	bytes_total = bytes_l1 + bytes_l2 + bytes_l3 + bytes_rem;

	printf("Overall bytes_l1 = %lld (%f %%)\n"
	       "Overall bytes_l2 = %lld (%f %%)\n"
	       "Overall bytes_l3 = %lld (%f %%)\n"
	       "Overall bytes_rem = %lld (%f %%)\n",
	       bytes_l1, 100.0*(double)bytes_l1/(double)bytes_total,
	       bytes_l2, 100.0*(double)bytes_l2/(double)bytes_total,
	       bytes_l3, 100.0*(double)bytes_l3/(double)bytes_total,
	       bytes_rem, 100.0*(double)bytes_rem/(double)bytes_total);
}

#endif
