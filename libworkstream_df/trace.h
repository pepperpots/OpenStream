#ifndef TRACE_H
#define TRACE_H

#include <stdint.h>

#include "config.h"

#define WQEVENT_STATECHANGE 0
#define WQEVENT_SINGLEEVENT 1
#define WQEVENT_STEAL 2
#define WQEVENT_TCREATE 3
#define WQEVENT_PUSH 4
#define WQEVENT_START_TASKEXEC 5
#define WQEVENT_END_TASKEXEC 6

#define WORKER_STATE_SEEKING 0
#define WORKER_STATE_TASKEXEC 1
#define WORKER_STATE_RT_TCREATE 2
#define WORKER_STATE_RT_RESDEP 3
#define WORKER_STATE_RT_TDEC 4
#define WORKER_STATE_RT_BCAST 5
#define WORKER_STATE_RT_INIT 6
#define WORKER_STATE_RT_ESTIMATE_COSTS 7
#define WORKER_STATE_MAX 8

typedef struct worker_event {
  uint64_t time;
  uint32_t type;

  union {
    struct {
      uint32_t src;
      uint32_t size;
    } steal;

    struct {
      uint16_t from_node;
      uint16_t type;
      uint64_t creation_timestamp;
      uint64_t ready_timestamp;
      uint64_t cache_misses;
      uint64_t allocator_cache_misses;
      uint32_t size;
    } texec;

    struct {
      uint32_t dst;
      uint32_t size;
    } push;

    struct {
      uint32_t state;
      uint32_t previous_state_idx;
    } state_change;
  };
} worker_state_change_t, *worker_state_change_p;

#if ALLOW_WQEVENT_SAMPLING

#define WSTREAM_DF_THREAD_EVENT_SAMPLING_FIELDS \
  worker_state_change_t events[MAX_WQEVENT_SAMPLES]; \
  unsigned int num_events; \
  unsigned int previous_state_idx;

struct wstream_df_thread;

void trace_init(struct wstream_df_thread* cthread);
void trace_event(struct wstream_df_thread* cthread, unsigned int type);
void trace_task_exec_start(struct wstream_df_thread* cthread, unsigned int from_node, unsigned int type, uint64_t creation_timestamp, uint64_t ready_timestamp, uint32_t size, uint64_t cache_misses, uint64_t allocator_cache_misses);
void trace_task_exec_end(struct wstream_df_thread* cthread);
void trace_state_change(struct wstream_df_thread* cthread, unsigned int state);
void trace_state_restore(struct wstream_df_thread* cthread);
void trace_steal(struct wstream_df_thread* cthread, unsigned int src, unsigned int size);
void trace_push(struct wstream_df_thread* cthread, unsigned int dst, unsigned int size);

void dump_events(int num_workers, struct wstream_df_thread* wstream_df_worker_threads);
void dump_average_task_duration_summary(int num_workers, struct wstream_df_thread* wstream_df_worker_threads);
void dump_average_task_duration(unsigned int num_intervals, int num_workers, struct wstream_df_thread* wstream_df_worker_threads);
void dump_task_duration_histogram(int num_workers, struct wstream_df_thread* wstream_df_worker_threads);
void dump_avg_state_parallelism(unsigned int state, uint64_t max_intervals, int num_workers, struct wstream_df_thread* wstream_df_worker_threads);

#else

#define WSTREAM_DF_THREAD_EVENT_SAMPLING_FIELDS

#define trace_init(cthread)  do { } while(0)
#define trace_task_exec_end(cthread) do { } while(0)
#define trace_task_exec_start(cthread, from_node, type, creation_timestamp, ready_timestamp, size, cache_misses, allocator_cache_misses) do { } while(0)
#define trace_event(cthread, type) do { } while(0)
#define trace_state_change(cthread, state) do { } while(0)
#define trace_steal(cthread, src, size) do { } while(0)
#define trace_push(cthread, src, size) do { } while(0)
#define trace_state_restore(cthread) do { } while(0)

#define dump_events(num_workers, wstream_df_worker_threads)  do { } while(0)
#define dump_average_task_duration_summary(num_workers, wstream_df_worker_threads) do { } while(0)
#define dump_average_task_duration(num_intervals, num_workers, wstream_df_worker_threads) do { } while(0)
#define dump_avg_state_parallelism(state, max_intervals, num_workers, wstream_df_worker_threads) do { } while(0)
#define dump_task_duration_histogram(num_workers, wstream_df_worker_threads) do { } while(0)
#endif

#endif
