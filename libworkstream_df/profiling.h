#ifndef PROFILING_H
#define PROFILING_H

#include "config.h"
#include <stdint.h>

#ifdef WS_PAPI_PROFILE
#include <papi.h>
#endif

struct wstream_df_thread;
struct wstream_df_numa_node;

#if ALLOW_PUSHES
#define WSTREAM_DF_THREAD_WQUEUE_PROFILE_PUSH_FIELDS \
	unsigned long long steals_pushed; \
	unsigned long long pushes_mem[MEM_NUM_LEVELS]; \
	unsigned long long pushes_fails;
#else
#define WSTREAM_DF_THREAD_WQUEUE_PROFILE_PUSH_FIELDS
#endif

#ifdef WS_PAPI_PROFILE

#define WSTREAM_DF_THREAD_PAPI_FIELDS \
	int papi_count; \
	long long papi_counters[WS_PAPI_NUM_EVENTS]; \
	long long papi_event_mapping[WS_PAPI_NUM_EVENTS]; \
	int papi_event_set; \
	int papi_num_events;

void
setup_papi(void);

void
update_papi(struct wstream_df_thread* th);

void
update_papi_timestamp(struct wstream_df_thread* th, int64_t timestamp);

void
init_papi(struct wstream_df_thread* th);
#else
#define WSTREAM_DF_THREAD_PAPI_FIELDS
#define setup_papi() do { } while(0)
#define init_papi(th) do { } while(0)
#define update_papi(th) do { } while(0)
#define update_papi_timestamp(th, ts) do { } while(0)
#endif

#if WQUEUE_PROFILE 
#define WSTREAM_DF_THREAD_WQUEUE_PROFILE_BASIC_FIELDS \
	unsigned long long steals_fails; \
	unsigned long long steals_owncached; \
	unsigned long long steals_ownqueue; \
	unsigned long long steals_mem[MEM_NUM_LEVELS]; \
	unsigned long long bytes_mem[MEM_NUM_LEVELS]; \
	unsigned long long tasks_created; \
	unsigned long long tasks_executed; \
	unsigned long long tasks_executed_localalloc; \
	unsigned long long reuse_addr; \
	unsigned long long reuse_copy; \

void
init_wqueue_counters (struct wstream_df_thread* th);

void
setup_wqueue_counters (void);

void
stop_wqueue_counters (void);

void
wqueue_counters_enter_runtime(struct wstream_df_thread* th);

void
dump_wqueue_counters (unsigned int num_workers, struct wstream_df_thread** wstream_df_worker_threads);

void
dump_global_wqueue_counters ();

#define inc_wqueue_counter(ctr, delta) \
	do { \
	(*(ctr)) += delta; \
	} while (0)

#define set_wqueue_counter(ctr, val) \
	do { \
	(*(ctr)) = val; \
	} while (0)

#define set_wqueue_counter_if_zero(ctr, val) \
	do { \
	if((*(ctr)) == 0) \
		(*(ctr)) = val; \
	} while (0)

#else

#define WSTREAM_DF_THREAD_WQUEUE_PROFILE_BASIC_FIELDS
#define init_wqueue_counters(th) do {} while(0)
#define setup_wqueue_counters() do {} while(0)
#define wqueue_counters_enter_runtime(th) do {} while(0)
#define stop_wqueue_counters() do {} while(0)
#define dump_wqueue_counters(num_workers, wstream_df_worker_threads) do {} while(0)
#define inc_wqueue_counter(ctr, delta) do {} while(0)
#define set_wqueue_counter(ctr, val) do {} while(0)
#define set_wqueue_counter_if_zero(ctr, val) do {} while(0)
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
	WSTREAM_DF_THREAD_WQUEUE_PROFILE_PUSH_FIELDS \
	WSTREAM_DF_THREAD_PAPI_FIELDS

#endif
