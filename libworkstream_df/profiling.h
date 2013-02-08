#ifndef PROFILING_H
#define PROFILING_H

#include "config.h"
#include <stdint.h>

#if ALLOW_PUSHES
#define WSTREAM_DF_THREAD_WQUEUE_PROFILE_PUSH_FIELDS \
	unsigned long long steals_pushed; \
	unsigned long long pushes_mem[MEM_NUM_LEVELS]; \
	unsigned long long pushes_fails;
#else
#define WSTREAM_DF_THREAD_WQUEUE_PROFILE_PUSH_FIELDS
#endif

#ifdef WQUEUE_PROFILE
#define WSTREAM_DF_THREAD_WQUEUE_PROFILE_BASIC_FIELDS \
	unsigned long long steals_fails; \
	unsigned long long steals_owncached; \
	unsigned long long steals_ownqueue; \
	unsigned long long steals_mem[MEM_NUM_LEVELS]; \
	unsigned long long bytes_mem[MEM_NUM_LEVELS]; \
	unsigned long long tasks_created; \
	unsigned long long tasks_executed;

struct wstream_df_thread;

void
init_wqueue_counters (struct wstream_df_thread* th);

void
dump_wqueue_counters (unsigned int num_workers, struct wstream_df_thread* wstream_df_worker_threads);

void
dump_global_wqueue_counters ();

#define inc_wqueue_counter(ctr, delta) \
	do { \
	(*(ctr)) += delta; \
	} while (0)

#else

#define WSTREAM_DF_THREAD_WQUEUE_PROFILE_BASIC_FIELDS
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

#define WSTREAM_DF_THREAD_WQUEUE_PROFILE_FIELDS \
	WSTREAM_DF_THREAD_WQUEUE_PROFILE_BASIC_FIELDS \
	WSTREAM_DF_THREAD_WQUEUE_PROFILE_PUSH_FIELDS

#endif
