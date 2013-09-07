#ifndef TRACE_H
#define TRACE_H

#include <stdint.h>

#include "config.h"
#include "trace_file.h"

#define WQEVENT_STATECHANGE 0
#define WQEVENT_SINGLEEVENT 1
#define WQEVENT_STEAL 2
#define WQEVENT_TCREATE 3
#define WQEVENT_PUSH 4
#define WQEVENT_START_TASKEXEC 5
#define WQEVENT_END_TASKEXEC 6
#define WQEVENT_DATA_READ 7
#define WQEVENT_COUNTER 8

typedef struct worker_event {
  uint64_t time;
  uint32_t type;
  uint32_t cpu;
  uint64_t active_task;
  uint64_t active_frame;

  union {
    struct {
      uint32_t src_worker;
      uint32_t src_cpu;
      uint32_t size;
      uint64_t what;
    } steal;

    struct {
      uint64_t frame;
    } tcreate;

    struct {
      uint64_t frame;
    } texec_start;

    struct {
      uint64_t frame;
    } texec_end;

    struct {
      uint16_t type;
      uint64_t creation_timestamp;
      uint64_t ready_timestamp;
    } texec;

    struct {
      uint32_t dst_worker;
      uint32_t dst_cpu;
      uint32_t size;
      uint64_t what;
    } push;

    struct {
      uint32_t src_cpu;
      uint32_t size;
      uint64_t prod_ts;
    } data_read;

    struct {
      uint64_t counter_id;
      int64_t value;
    } counter;

    struct {
      enum worker_state state;
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
struct wstream_df_frame;

void trace_init(struct wstream_df_thread* cthread);
void trace_event(struct wstream_df_thread* cthread, unsigned int type);
void trace_tcreate(struct wstream_df_thread* cthread, struct wstream_df_frame* frame);
void trace_task_exec_start(struct wstream_df_thread* cthread, struct wstream_df_frame* frame);
void trace_task_exec_end(struct wstream_df_thread* cthread, struct wstream_df_frame* frame);
void trace_state_change(struct wstream_df_thread* cthread, unsigned int state);
void trace_state_restore(struct wstream_df_thread* cthread);
void trace_steal(struct wstream_df_thread* cthread, unsigned int src_worker, unsigned int src_cpu, unsigned int size, void* frame);
void trace_push(struct wstream_df_thread* cthread, unsigned int dst_worker, unsigned int dst_cpu, unsigned int size, void* frame);
void trace_data_read(struct wstream_df_thread* cthread, unsigned int src_cpu, unsigned int size, long long prod_ts);
void trace_counter(struct wstream_df_thread* cthread, uint64_t counter_id, int64_t value);

void dump_events_ostv(int num_workers, struct wstream_df_thread* wstream_df_worker_threads);
#else

#define WSTREAM_DF_THREAD_EVENT_SAMPLING_FIELDS

#define trace_init(cthread) do { } while(0)
#define trace_tcreate(cthread, frame) do { } while(0)
#define trace_task_exec_end(cthread, frame) do { } while(0)
#define trace_task_exec_start(cthread, frame) do { } while(0)
#define trace_event(cthread, type) do { } while(0)
#define trace_state_change(cthread, state) do { } while(0)
#define trace_steal(cthread, src_worker, src_cpu, size, fp) do { } while(0)
#define trace_push(cthread, dst_worker, dst_cpu, size, fp) do { } while(0)
#define trace_state_restore(cthread) do { } while(0)
#define trace_data_read(cthread, src_cpu, size) do { } while(0)
#define trace_counter(cthread, counter_id, value) do { } while(0)

#define dump_events_ostv(num_workers, wstream_df_worker_threads)  do { } while(0)
#endif

#endif
