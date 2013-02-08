#ifndef PROFILING_H
#define PROFILING_H

#include "config.h"
#include "wstream_df.h"

#ifdef WQUEUE_PROFILE
void
init_wqueue_counters (wstream_df_thread_p th);

void
dump_wqueue_counters (unsigned int num_workers, wstream_df_thread_p wstream_df_worker_threads);

void
dump_global_wqueue_counters ();

#define inc_wqueue_counter(ctr, delta) \
	do { \
	(*(ctr)) += delta; \
	} while (0)

#else
#define init_wqueue_counters(th) do {} while(0)
#define dump_wqueue_counters(num_workers, wstream_df_worker_threads) do {} while(0)
#define inc_wqueue_counter(ctr, delta) do {} while(0)
#endif

#ifdef MATRIX_PROFILE
extern unsigned long long transfer_matrix[MAX_CPUS][MAX_CPUS];

static inline void
inc_transfer_matrix_entry(unsigned int consumer, unsigned int producer,
			  unsigned long long num_bytes)
{
	transfer_matrix[consumer][producer] += num_bytes;
}

void init_transfer_matrix(void);
void dump_transfer_matrix(unsigned int num_workers);
#else
#define inc_transfer_matrix_entry(consumer, producer, num_bytes) do {} while(0)
#define init_transfer_matrix() do {} while(0)
#define dump_transfer_matrix(num_workers) do {} while(0)
#endif

#endif
