#ifndef TRACE_H
#define TRACE_H

#include <stdint.h>

#include "config.h"
#include "trace_file.h"
#include "convert.h"
#include "wstream_df.h"

#define WQEVENT_STATECHANGE 0
#define WQEVENT_SINGLEEVENT 1
#define WQEVENT_STEAL 2
#define WQEVENT_TCREATE 3
#define WQEVENT_PUSH 4
#define WQEVENT_START_TASKEXEC 5
#define WQEVENT_END_TASKEXEC 6
#define WQEVENT_DATA_READ 7
#define WQEVENT_DATA_WRITE 8
#define WQEVENT_COUNTER 9
#define WQEVENT_FRAME_INFO 10
#define WQEVENT_TDESTROY 11
#define WQEVENT_MEASURE_START 12
#define WQEVENT_MEASURE_END 13

#define PAPI_COUNTER_BASE 1000
#define RUNTIME_COUNTER_BASE 2000

enum runtime_counter_ids {
  RUNTIME_COUNTER_WQLENGTH = 0,
  RUNTIME_COUNTER_STEALS,
  RUNTIME_COUNTER_PUSHES,
  RUNTIME_COUNTER_NTCREATE,
  RUNTIME_COUNTER_NTEXEC,
  RUNTIME_COUNTER_SLAB_REFILLS,
  RUNTIME_COUNTER_REUSE_ADDR,
  RUNTIME_COUNTER_REUSE_COPY,
  RUNTIME_COUNTER_SYSTEM_TIME_US,
  RUNTIME_COUNTER_MAJOR_PAGE_FAULTS,
  RUNTIME_COUNTER_MINOR_PAGE_FAULTS,
  RUNTIME_COUNTER_MAX_RESIDENT_SIZE,
  RUNTIME_COUNTER_INV_CONTEXT_SWITCHES,
  NUM_RUNTIME_COUNTERS
};

typedef struct worker_event {
  int64_t time;
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
    } tdestroy;

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
      uint64_t src_addr;
    } data_read;

    struct {
      uint32_t size;
      uint64_t dst_addr;
    } data_write;

    struct {
      uint64_t counter_id;
      int64_t value;
    } counter;

    struct {
      enum worker_state state;
      uint32_t previous_state_idx;
    } state_change;

    struct {
      uint64_t addr;
      int32_t numa_node;
      uint32_t size;
    } frame_info;
  };
} worker_state_change_t, *worker_state_change_p;

#if ALLOW_WQEVENT_SAMPLING

//[MAX_WQEVENT_SAMPLES];

struct wstream_df_thread;
struct wstream_df_frame;

#include "trace_inline.h"


void trace_init(struct wstream_df_thread* cthread);
void trace_counter_timestamp(struct wstream_df_thread* cthread, uint64_t counter_id, int64_t value, int64_t timestamp);
void trace_frame_info(struct wstream_df_thread* cthread, struct wstream_df_frame* frame);
void trace_measure_start(struct wstream_df_thread* cthread);
void trace_measure_end(struct wstream_df_thread* cthread);

void dump_events_ostv(int num_workers, struct wstream_df_thread** wstream_df_worker_threads);
#else

#define trace_init(cthread) do { } while(0)
#define trace_tcreate(cthread, frame) do { } while(0)
#define trace_tdestroy(cthread, frame) do { } while(0)
#define trace_update_tcreate_fp(cthread, frame) do { } while(0)
#define trace_task_exec_end(cthread, frame) do { } while(0)
#define trace_task_exec_start(cthread, frame) do { } while(0)
#define trace_event(cthread, type) do { } while(0)
#define trace_state_change(cthread, state) do { } while(0)
#define trace_steal(cthread, src_worker, src_cpu, size, fp) do { } while(0)
#define trace_push(cthread, dst_worker, dst_cpu, size, fp) do { } while(0)
#define trace_state_restore(cthread) do { } while(0)
#define trace_data_read(cthread, src_cpu, size, prod_ts, addr) do { } while(0)
#define trace_data_write(cthread, size, dst_addr)  do { } while(0)
#define trace_counter(cthread, counter_id, value) do { } while(0)
#define trace_counter_timestamp(cthread, counter_id, value, timestamp) do { } while(0)
#define trace_frame_info(cthread, frame) do { } while(0)
#define trace_measure_start(cthread) do { } while(0)
#define trace_measure_end(cthread) do { } while(0)

#define dump_events_ostv(num_workers, wstream_df_worker_threads) do { } while(0)
#endif

#if ALLOW_WQEVENT_SAMPLING && TRACE_QUEUE_STATS && WQUEUE_PROFILE
void trace_runtime_counters(struct wstream_df_thread* cthread);
#else
#define trace_runtime_counters(cthread) do { } while(0)
#endif

#endif
