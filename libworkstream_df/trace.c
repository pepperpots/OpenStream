#include <inttypes.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <string.h>

#include "trace.h"
#include "wstream_df.h"
#include "arch.h"
#include "numa.h"

int wstream_df_alloc_on_node(void* p, size_t size, int node);

static const char* runtime_counter_names[NUM_RUNTIME_COUNTERS] = {
  "wq_length",
  "wq_steals",
  "wq_pushes",
  "num_tcreate",
  "num_texec",
  "slab_refills",
  "reuse_addr",
  "reuse_copy"
};

#if ALLOW_WQEVENT_SAMPLING
static const char* state_names[] = {
  "seeking",
  "taskexec",
  "tcreate",
  "resdep",
  "tdec",
  "broadcast",
  "init",
  "estimate_costs",
  "reorder"
};

void trace_init(struct wstream_df_thread* cthread)
{
	size_t size = MAX_WQEVENT_SAMPLES*sizeof(cthread->events[0]);

	cthread->num_events = 0;
	cthread->previous_state_idx = 0;
	cthread->events = malloc(size);

	if(!cthread->events) {
		exit(1);
	}

	wstream_df_alloc_on_node(cthread->events, size, cthread->numa_node->id);
	//slab_force_advise_pages(cthread->events, size, MADV_HUGEPAGE);
}

void trace_frame_info(struct wstream_df_thread* cthread, struct wstream_df_frame* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].type = WQEVENT_FRAME_INFO;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->events[cthread->num_events].frame_info.addr = (uint64_t)frame;
  cthread->events[cthread->num_events].frame_info.numa_node = wstream_numa_node_of(frame);
  cthread->events[cthread->num_events].frame_info.size = frame->size;
  cthread->num_events++;
}

void trace_event(wstream_df_thread_p cthread, unsigned int type)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].type = type;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->num_events++;
}

void trace_update_tcreate_fp(struct wstream_df_thread* cthread, struct wstream_df_frame* frame)
{
  cthread->last_tcreate_event->tcreate.frame = frame;
  cthread->last_tcreate_event = NULL;
}

void trace_tcreate(struct wstream_df_thread* cthread, struct wstream_df_frame* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].type = WQEVENT_TCREATE;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->events[cthread->num_events].tcreate.frame = (uint64_t)frame;
  cthread->last_tcreate_event = &cthread->events[cthread->num_events];
  cthread->num_events++;
}

void trace_measure(struct wstream_df_thread* cthread, int type)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);

  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].type = type;
  cthread->num_events++;
}

void trace_measure_start(struct wstream_df_thread* cthread)
{
  trace_measure(cthread, WQEVENT_MEASURE_START);
}

void trace_measure_end(struct wstream_df_thread* cthread)
{
  trace_measure(cthread, WQEVENT_MEASURE_END);
}

void trace_tdestroy(struct wstream_df_thread* cthread, struct wstream_df_frame* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].type = WQEVENT_TDESTROY;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->events[cthread->num_events].tdestroy.frame = (uint64_t)frame;
  cthread->num_events++;
}

void trace_task_exec_start(wstream_df_thread_p cthread, struct wstream_df_frame* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].texec.type = frame->steal_type;
  cthread->events[cthread->num_events].texec.creation_timestamp = frame->creation_timestamp;
  cthread->events[cthread->num_events].texec.ready_timestamp = frame->ready_timestamp;
  cthread->events[cthread->num_events].type = WQEVENT_START_TASKEXEC;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->events[cthread->num_events].texec_start.frame = (uint64_t)frame;
  cthread->num_events++;
}

void trace_task_exec_end(wstream_df_thread_p cthread, struct wstream_df_frame* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].type = WQEVENT_END_TASKEXEC;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->events[cthread->num_events].texec_end.frame = (uint64_t)frame;
  cthread->num_events++;
}

void trace_state_change(wstream_df_thread_p cthread, unsigned int state)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].state_change.state = state;
  cthread->events[cthread->num_events].type = WQEVENT_STATECHANGE;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;

  cthread->events[cthread->num_events].state_change.previous_state_idx =
    cthread->previous_state_idx;

  cthread->previous_state_idx = cthread->num_events;
  cthread->num_events++;
}

void trace_state_restore(wstream_df_thread_p cthread)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].type = WQEVENT_STATECHANGE;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;

  cthread->events[cthread->num_events].state_change.state =
    cthread->events[cthread->events[cthread->previous_state_idx].state_change.previous_state_idx].state_change.state;

  cthread->events[cthread->num_events].state_change.previous_state_idx =
    cthread->events[cthread->previous_state_idx].state_change.previous_state_idx;

  cthread->previous_state_idx = cthread->events[cthread->previous_state_idx].state_change.previous_state_idx; //cthread->num_events;
  cthread->num_events++;
}

void trace_steal(wstream_df_thread_p cthread, unsigned int src_worker, unsigned int src_cpu, unsigned int size, void* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].steal.src_worker = src_worker;
  cthread->events[cthread->num_events].steal.src_cpu = src_cpu;
  cthread->events[cthread->num_events].steal.size = size;
  cthread->events[cthread->num_events].steal.what = (uint64_t)frame;
  cthread->events[cthread->num_events].type = WQEVENT_STEAL;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->num_events++;
}

void trace_push(wstream_df_thread_p cthread, unsigned int dst_worker, unsigned int dst_cpu, unsigned int size, void* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].push.dst_worker = dst_worker;
  cthread->events[cthread->num_events].push.dst_cpu = dst_cpu;
  cthread->events[cthread->num_events].push.size = size;
  cthread->events[cthread->num_events].steal.what = (uint64_t)frame;
  cthread->events[cthread->num_events].type = WQEVENT_PUSH;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->num_events++;
}

void trace_data_read(struct wstream_df_thread* cthread, unsigned int src_cpu, unsigned int size, long long prod_ts, void* src_addr)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].data_read.src_cpu = src_cpu;
  cthread->events[cthread->num_events].data_read.size = size;
  cthread->events[cthread->num_events].data_read.prod_ts = prod_ts;
  cthread->events[cthread->num_events].data_read.src_addr = (uint64_t)src_addr;
  cthread->events[cthread->num_events].type = WQEVENT_DATA_READ;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->num_events++;
}

void trace_counter_timestamp(struct wstream_df_thread* cthread, uint64_t counter_id, int64_t value, int64_t timestamp)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = timestamp - cthread->tsc_offset;
  cthread->events[cthread->num_events].counter.counter_id = counter_id;
  cthread->events[cthread->num_events].counter.value = value;
  cthread->events[cthread->num_events].type = WQEVENT_COUNTER;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->num_events++;
}

void trace_counter(struct wstream_df_thread* cthread, uint64_t counter_id, int64_t value)
{
  trace_counter_timestamp(cthread, counter_id, value, rdtsc());
}

#if ALLOW_WQEVENT_SAMPLING && defined(TRACE_QUEUE_STATS) && WQUEUE_PROFILE
void trace_runtime_counters(struct wstream_df_thread* cthread)
{
  trace_counter(cthread, RUNTIME_COUNTER_BASE+RUNTIME_COUNTER_WQLENGTH, cthread->work_deque.bottom - cthread->work_deque.top);
  trace_counter(cthread, RUNTIME_COUNTER_BASE+RUNTIME_COUNTER_NTCREATE, cthread->tasks_created);
  trace_counter(cthread, RUNTIME_COUNTER_BASE+RUNTIME_COUNTER_NTEXEC, cthread->tasks_executed);
  trace_counter(cthread, RUNTIME_COUNTER_BASE+RUNTIME_COUNTER_SLAB_REFILLS, cthread->slab_cache->slab_refills);
  trace_counter(cthread, RUNTIME_COUNTER_BASE+RUNTIME_COUNTER_REUSE_ADDR, cthread->reuse_addr);
  trace_counter(cthread, RUNTIME_COUNTER_BASE+RUNTIME_COUNTER_REUSE_COPY, cthread->reuse_copy);

  uint64_t steals = 0;
  for(int level = 0; level < MEM_NUM_LEVELS; level++)
    steals += cthread->steals_mem[level];

  trace_counter(cthread, RUNTIME_COUNTER_BASE+RUNTIME_COUNTER_STEALS, steals);

#if ALLOW_PUSHES
  uint64_t pushes = 0;
  for(int level = 0; level < MEM_NUM_LEVELS; level++)
    pushes += cthread->pushes_mem[level];

  trace_counter(cthread, RUNTIME_COUNTER_BASE+RUNTIME_COUNTER_PUSHES, pushes);
#endif
}
#endif

int get_next_event(wstream_df_thread_p th, int curr, unsigned int type)
{
  for(curr = curr+1; (unsigned int)curr < th->num_events; curr++) {
    if(th->events[curr].type == type)
      return curr;
  }

  return -1;
}

int get_next_state_change(wstream_df_thread_p th, int curr)
{
  return get_next_event(th, curr, WQEVENT_STATECHANGE);
}

int get_min_index(int* curr_idx, int num_workers, wstream_df_thread_p wstream_df_worker_threads)
{
  int i;
  int min_idx = -1;
  uint64_t min = UINT64_MAX;
  uint64_t curr;

  for(i = 0; i < num_workers; i++) {
    if(curr_idx[i] != -1) {
      curr = wstream_df_worker_threads[i].events[curr_idx[i]].time;

      if(curr < min) {
	min = curr;
	min_idx = i;
      }
    }
  }

  return min_idx;
}

int64_t get_min_time(int num_workers, wstream_df_thread_p* wstream_df_worker_threads)
{
  int i;
  int64_t min = -1;
  int initialized = 0;

  for(i = 0; i < num_workers; i++) {
    if(wstream_df_worker_threads[i]->num_events > 0) {
      if(!initialized || wstream_df_worker_threads[i]->events[0].time < min) {
	initialized = 1;
	min = wstream_df_worker_threads[i]->events[0].time;
      }
    }
  }


  return min;
}

int64_t get_max_time(int num_workers, wstream_df_thread_p* wstream_df_worker_threads)
{
  int i;
  int64_t max = -1;
  int initialized = 0;

  for(i = 0; i < num_workers; i++) {
    if(wstream_df_worker_threads[i]->num_events > 0) {
      if(initialized == 0 || wstream_df_worker_threads[i]->events[wstream_df_worker_threads[i]->num_events-1].time > max) {
	initialized = 1;
	max = wstream_df_worker_threads[i]->events[wstream_df_worker_threads[i]->num_events-1].time;
      }
    }
  }

  return max;
}

int64_t get_state_duration(wstream_df_thread_p th, int* idx, int max_idx)
{
  int new_idx;
  int64_t duration = 0;

  new_idx = get_next_state_change(th, *idx);

  if(new_idx == -1)
    new_idx = th->num_events-1;

  if(new_idx < max_idx) {
    duration = th->events[new_idx].time - th->events[*idx].time;
    *idx = new_idx;
    return duration;
  }

  return -1;
}

void get_task_duration_stats(int num_workers, wstream_df_thread_p wstream_df_worker_threads,
			     uint64_t* total_duration, uint64_t* num_tasks, uint64_t* max_duration,
			     uint64_t* max_create_to_exec_time, uint64_t* max_create_to_ready_time,
			     uint64_t* max_ready_to_exec_time)
{
  uint64_t duration, create_to_exec_time, create_to_ready_time, ready_to_exec_time;
  int64_t state_duration;
  int i, idx_start, idx_end, idx_tmp;
  wstream_df_thread_p th;

  *max_duration = 0;
  *num_tasks = 0;
  *total_duration = 0;
  *max_ready_to_exec_time = 0;
  *max_create_to_exec_time = 0;
  *max_create_to_ready_time = 0;

  for(i = 0; i < num_workers; i++) {
    idx_start = -1;

    th = &wstream_df_worker_threads[i];

    while((idx_start = get_next_event(th, idx_start, WQEVENT_START_TASKEXEC)) != -1) {
      idx_end = get_next_event(th, idx_start, WQEVENT_END_TASKEXEC);
      if(idx_end != -1) {
	create_to_exec_time = th->events[idx_start].time - th->events[idx_start].texec.creation_timestamp;
	create_to_ready_time = th->events[idx_start].texec.ready_timestamp - th->events[idx_start].texec.creation_timestamp;
	ready_to_exec_time = th->events[idx_start].time - th->events[idx_start].texec.ready_timestamp;
	duration = 0;
	idx_tmp = idx_start+1;

	while((state_duration = get_state_duration(th, &idx_tmp, idx_end)) != -1)
	  duration += state_duration;

	*total_duration += duration;
	(*num_tasks)++;

	if(duration > *max_duration)
	  *max_duration = duration;

	if(create_to_exec_time > *max_create_to_exec_time)
	  *max_create_to_exec_time = create_to_exec_time;

	if(create_to_ready_time > *max_create_to_ready_time)
	  *max_create_to_ready_time = create_to_ready_time;

	if(ready_to_exec_time > *max_ready_to_exec_time)
	  *max_ready_to_exec_time = ready_to_exec_time;

	idx_start = idx_end;
      }
    }
  }
}

int conditional_fprintf(int do_dump, FILE* fp, const char *format, ...)
{
  va_list args;
  int ret = 0;

  if(do_dump) {
    va_start(args, format);
    ret = vfprintf(fp, format, args);
    va_end(args);
  }

  return ret;
}

/* Dumps worker events to a file in paraver format. */
void dump_events_ostv(int num_workers, wstream_df_thread_p* wstream_df_worker_threads)
{
  unsigned int i, k;
  wstream_df_thread_p th;
  time_t t = time(NULL);
  struct tm * now = localtime(&t);
  int64_t max_time = get_max_time(num_workers, wstream_df_worker_threads);
  int64_t min_time = get_min_time(num_workers, wstream_df_worker_threads);
  FILE* fp = fopen(WQEVENT_SAMPLING_OUTFILE, "w+");
  int last_state_idx;
  unsigned int state;
  unsigned long long state_durations[WORKER_STATE_MAX];
  unsigned long long total_duration = 0;

  struct trace_header dsk_header;
  struct trace_state_event dsk_se;
  struct trace_comm_event dsk_ce;
  struct trace_single_event dsk_sge;
  struct trace_counter_event dsk_cre;
  struct trace_frame_info dsk_fi;
  struct trace_cpu_info dsk_ci;
  struct trace_global_single_event dsk_gse;

  int do_dump;

  assert(fp != NULL);

  memset(state_durations, 0, sizeof(state_durations));

  assert(min_time != -1);
  assert(max_time != -1);

  /* Write header */
  dsk_header.magic = TRACE_MAGIC;
  dsk_header.version = TRACE_VERSION;
  dsk_header.day = now->tm_mday;
  dsk_header.month = now->tm_mon+1;
  dsk_header.year = now->tm_year+1900;
  dsk_header.hour = now->tm_hour;
  dsk_header.minute = now->tm_min;

  write_struct_convert(fp, &dsk_header, sizeof(dsk_header), trace_header_conversion_table, 0);

#ifdef TRACE_PAPI_COUNTERS
  const char* events[] = WS_PAPI_EVENTS;

  for(int i = 0; i < WS_PAPI_NUM_EVENTS; i++) {
    struct trace_counter_description dsk_cd;
    int name_len = strlen(events[i]);

    dsk_cd.type = EVENT_TYPE_COUNTER_DESCRIPTION;
    dsk_cd.name_len = name_len;
    dsk_cd.counter_id = i+PAPI_COUNTER_BASE;

    write_struct_convert(fp, &dsk_cd, sizeof(dsk_cd), trace_counter_description_conversion_table, 0);

    fwrite(events[i], name_len, 1, fp);
  }
#endif

#ifdef TRACE_QUEUE_STATS
  for(int i = 0; i < NUM_RUNTIME_COUNTERS; i++) {
    struct trace_counter_description dsk_cd;
    int name_len = strlen(runtime_counter_names[i]);

    dsk_cd.type = EVENT_TYPE_COUNTER_DESCRIPTION;
    dsk_cd.name_len = name_len;
    dsk_cd.counter_id = i+RUNTIME_COUNTER_BASE;

    write_struct_convert(fp, &dsk_cd, sizeof(dsk_cd), trace_counter_description_conversion_table, 0);

    fwrite(runtime_counter_names[i], name_len, 1, fp);
  }
#endif

  /* Dump events and states */
  for (i = 0; i < (unsigned int)num_workers; ++i) {
    th = wstream_df_worker_threads[i];
    last_state_idx = -1;
    do_dump = 1;

    /* Write CPU info */
    dsk_ci.header.type = EVENT_TYPE_CPU_INFO;
    dsk_ci.header.cpu = th->cpu;
    dsk_ci.numa_node = th->numa_node->id;

    write_struct_convert(fp, &dsk_ci, sizeof(dsk_ci), trace_cpu_info_conversion_table, 0);

    if(th->num_events > 0) {
      for(k = 0; k < th->num_events; k++) {
	if(MAX_WQEVENT_PARAVER_CYCLES != -1 &&
	   th->events[k].time-min_time > (int64_t)MAX_WQEVENT_PARAVER_CYCLES)
	  {
	    do_dump = 0;
	  }

	/* States */
	if(th->events[k].type == WQEVENT_STATECHANGE) {
	  if(last_state_idx != -1) {
	    state = th->events[last_state_idx].state_change.state;

	    if(do_dump) {
	      /* Not the first state change, so using last_state_idx is safe */
	      dsk_se.header.type = EVENT_TYPE_STATE;
	      dsk_se.header.time = th->events[last_state_idx].time-min_time;
	      dsk_se.header.cpu = th->events[last_state_idx].cpu;
	      dsk_se.header.worker = th->worker_id;
	      dsk_se.header.active_task = th->events[last_state_idx].active_task;
	      dsk_se.header.active_frame = th->events[last_state_idx].active_frame;
	      dsk_se.state = state;
	      dsk_se.end_time = th->events[k].time-min_time;
	      write_struct_convert(fp, &dsk_se, sizeof(dsk_se), trace_state_event_conversion_table, 0);
	    }

	    state_durations[state] += th->events[k].time - th->events[last_state_idx].time;
	    total_duration += th->events[k].time - th->events[last_state_idx].time;
	  } else {
#ifdef TRACE_RT_INIT_STATE
	    /* First state change, by default the initial state is "initialization" */
	    if(do_dump) {
	      dsk_se.header.type = EVENT_TYPE_STATE;
	      dsk_se.header.time = 0;
	      dsk_se.header.cpu = th->events[k].cpu;
	      dsk_se.header.worker = th->worker_id;
	      dsk_se.header.active_task = 0;
	      dsk_se.header.active_frame = 0;
	      dsk_se.state = WORKER_STATE_RT_INIT;
	      dsk_se.end_time = th->events[k].time-min_time;

	      write_struct_convert(fp, &dsk_se, sizeof(dsk_se), trace_state_event_conversion_table, 0);
	    }
#else
	    /* First state change, by default the initial state is "seeking" */
	    if(do_dump) {
	      dsk_se.header.type = EVENT_TYPE_STATE;
	      dsk_se.header.time = 0;
	      dsk_se.header.cpu = th->events[k].cpu;
	      dsk_se.header.worker = th->worker_id;
	      dsk_se.header.active_task = 0;
	      dsk_se.header.active_frame = 0;
	      dsk_se.state = WORKER_STATE_SEEKING;
	      dsk_se.end_time = th->events[k].time-min_time;

	      write_struct_convert(fp, &dsk_se, sizeof(dsk_se), trace_state_event_conversion_table, 0);
	    }
#endif
	    state_durations[WORKER_STATE_SEEKING] += th->events[k].time-min_time;
	    total_duration += th->events[k].time-min_time;
	  }

	  last_state_idx = k;
	} else if(th->events[k].type == WQEVENT_STEAL) {
	  /* Steal events (dumped as communication) */
	  if(do_dump) {
	    dsk_ce.header.type = EVENT_TYPE_COMM;
	    dsk_ce.header.time = th->events[k].time-min_time;
	    dsk_ce.header.cpu = th->events[k].cpu;
	    dsk_ce.header.worker = th->events[k].steal.src_worker;
	    dsk_ce.header.active_task = th->events[k].active_task;
	    dsk_ce.header.active_frame = th->events[k].active_frame;
	    dsk_ce.type = COMM_TYPE_STEAL;
	    dsk_ce.src_or_dst_cpu = th->events[k].steal.src_cpu;
	    dsk_ce.size = th->events[k].steal.size;
	    dsk_ce.what = th->events[k].steal.what;

	    write_struct_convert(fp, &dsk_ce, sizeof(dsk_ce), trace_comm_event_conversion_table, 0);
	  }
	} else if(th->events[k].type == WQEVENT_DATA_READ) {
	  /* Data read events (dumped as communication) */
	  if(do_dump) {
	    dsk_ce.header.type = EVENT_TYPE_COMM;
	    dsk_ce.header.time = th->events[k].time-min_time;
	    dsk_ce.header.cpu = th->events[k].cpu;
	    dsk_ce.header.active_task = th->events[k].active_task;
	    dsk_ce.header.active_frame = th->events[k].active_frame;
	    dsk_ce.type = COMM_TYPE_DATA_READ;
	    dsk_ce.src_or_dst_cpu = th->events[k].data_read.src_cpu;
	    dsk_ce.size = th->events[k].data_read.size;
	    dsk_ce.what = th->events[k].data_read.src_addr;
	    dsk_ce.prod_ts = th->events[k].data_read.prod_ts-min_time;

	    write_struct_convert(fp, &dsk_ce, sizeof(dsk_ce), trace_comm_event_conversion_table, 0);
	  }
	} else if(th->events[k].type == WQEVENT_DATA_WRITE) {
	  /* Data write events (dumped as communication) */
	  if(do_dump) {
	    dsk_ce.header.type = EVENT_TYPE_COMM;
	    dsk_ce.header.time = th->events[k].time-min_time;
	    dsk_ce.header.cpu = th->events[k].cpu;
	    dsk_ce.header.active_task = th->events[k].active_task;
	    dsk_ce.header.active_frame = th->events[k].active_frame;
	    dsk_ce.type = COMM_TYPE_DATA_WRITE;
	    dsk_ce.size = th->events[k].data_write.size;
	    dsk_ce.what = th->events[k].data_write.dst_addr;

	    write_struct_convert(fp, &dsk_ce, sizeof(dsk_ce), trace_comm_event_conversion_table, 0);
	  }
	} else if(th->events[k].type == WQEVENT_PUSH) {
	  /* Push events (dumped as communication) */
	  if(do_dump) {
	    dsk_ce.header.type = EVENT_TYPE_COMM;
	    dsk_ce.header.time = th->events[k].time-min_time;
	    dsk_ce.header.cpu = th->events[k].cpu;
	    dsk_ce.header.worker = th->worker_id;
	    dsk_ce.header.active_task = th->events[k].active_task;
	    dsk_ce.header.active_frame = th->events[k].active_frame;
	    dsk_ce.type = COMM_TYPE_PUSH;
	    dsk_ce.src_or_dst_cpu = th->events[k].push.dst_cpu;
	    dsk_ce.size = th->events[k].push.size;
	    dsk_ce.what = th->events[k].push.what;

	    write_struct_convert(fp, &dsk_ce, sizeof(dsk_ce), trace_comm_event_conversion_table, 0);
	  }
	} else if(th->events[k].type == WQEVENT_TCREATE ||
		  th->events[k].type == WQEVENT_START_TASKEXEC ||
		  th->events[k].type == WQEVENT_END_TASKEXEC ||
		  th->events[k].type == WQEVENT_TDESTROY) {
	  /* Tcreate event (simply dumped as an event) */

	  if(do_dump) {
	    dsk_sge.header.type = EVENT_TYPE_SINGLE;
	    dsk_sge.header.time = th->events[k].time-min_time;
	    dsk_sge.header.cpu = th->events[k].cpu;
	    dsk_sge.header.worker = th->worker_id;
	    dsk_sge.header.active_task = th->events[k].active_task;
	    dsk_sge.header.active_frame = th->events[k].active_frame;

	    if(th->events[k].type == WQEVENT_TCREATE) {
	      dsk_sge.type = SINGLE_TYPE_TCREATE;
	      dsk_sge.what = th->events[k].tcreate.frame;
	    } else if(th->events[k].type == WQEVENT_START_TASKEXEC) {
	      dsk_sge.type = SINGLE_TYPE_TEXEC_START;
	      dsk_sge.what = th->events[k].texec_start.frame;
	    } else if(th->events[k].type == WQEVENT_END_TASKEXEC) {
	      dsk_sge.type = SINGLE_TYPE_TEXEC_END;
	      dsk_sge.what = th->events[k].texec_end.frame;
	    } else if(th->events[k].type == WQEVENT_TDESTROY) {
	      dsk_sge.type = SINGLE_TYPE_TDESTROY;
	      dsk_sge.what = th->events[k].tdestroy.frame;
	    }

	    write_struct_convert(fp, &dsk_sge, sizeof(dsk_sge), trace_single_event_conversion_table, 0);
	  }
	} else if(th->events[k].type == WQEVENT_COUNTER) {
	  if(do_dump) {
	    dsk_cre.header.type = EVENT_TYPE_COUNTER;
	    dsk_cre.header.time = th->events[k].time-min_time;
	    dsk_cre.header.cpu = th->events[k].cpu;
	    dsk_cre.header.worker = th->worker_id;
	    dsk_cre.header.active_task = th->events[k].active_task;
	    dsk_cre.header.active_frame = th->events[k].active_frame;
	    dsk_cre.counter_id = th->events[k].counter.counter_id;
	    dsk_cre.value = th->events[k].counter.value;

	    write_struct_convert(fp, &dsk_cre, sizeof(dsk_cre), trace_counter_event_conversion_table, 0);
	  }
	} else if(th->events[k].type == WQEVENT_FRAME_INFO) {
	  if(do_dump) {
	    dsk_fi.header.type = EVENT_TYPE_FRAME_INFO;
	    dsk_fi.header.time = th->events[k].time-min_time;
	    dsk_fi.header.cpu = th->events[k].cpu;
	    dsk_fi.header.worker = th->worker_id;
	    dsk_fi.header.active_task = th->events[k].active_task;
	    dsk_fi.header.active_frame = th->events[k].active_frame;
	    dsk_fi.addr = th->events[k].frame_info.addr;
	    dsk_fi.numa_node = th->events[k].frame_info.numa_node;
	    dsk_fi.size = th->events[k].frame_info.size;

	    write_struct_convert(fp, &dsk_fi, sizeof(dsk_fi), trace_frame_info_conversion_table, 0);
	  }
	} else if(th->events[k].type == WQEVENT_MEASURE_START ||
		  th->events[k].type == WQEVENT_MEASURE_END)
	  {
	  if(do_dump) {
	    dsk_gse.type = EVENT_TYPE_GLOBAL_SINGLE_EVENT;
	    dsk_gse.time = th->events[k].time-min_time;
	    dsk_gse.single_type = (th->events[k].type == WQEVENT_MEASURE_START) ?
	      GLOBAL_SINGLE_TYPE_MEASURE_START :
	      GLOBAL_SINGLE_TYPE_MEASURE_END;

	    write_struct_convert(fp, &dsk_gse, sizeof(dsk_gse), trace_global_single_event_conversion_table, 0);
	  }
	}
      }

      /* Final state is "seeking" (beginning at the last state,
       * finishing at program termination) */
      if(do_dump) {
	dsk_se.header.type = EVENT_TYPE_STATE;
	dsk_se.header.time = th->events[last_state_idx].time-min_time;
	dsk_se.header.cpu = th->events[k-1].cpu;
	dsk_se.header.worker = th->worker_id;
	dsk_se.header.active_task = th->events[k-1].active_task;
	dsk_se.header.active_frame = th->events[k-1].active_frame;
	dsk_se.state = WORKER_STATE_SEEKING;
	dsk_se.end_time = max_time-min_time;

	write_struct_convert(fp, &dsk_se, sizeof(dsk_se), trace_state_event_conversion_table, 0);
      }

      state_durations[WORKER_STATE_SEEKING] += max_time-th->events[last_state_idx].time;
      total_duration += max_time-th->events[last_state_idx].time;
    }
  }

#if 0
  for(i = 0; i < WORKER_STATE_MAX; i++) {
    printf("Overall time for state %s: %lld (%.6f %%)\n",
	   state_names[i],
	   state_durations[i],
	   ((double)state_durations[i] / (double)total_duration)*100.0);
  }
#endif
  fclose(fp);
}

#endif

#if ALLOW_WQEVENT_SAMPLING
void trace_data_write(struct wstream_df_thread* cthread, unsigned int size, uint64_t dst_addr)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc() - cthread->tsc_offset;
  cthread->events[cthread->num_events].data_write.size = size;
  cthread->events[cthread->num_events].data_write.dst_addr = dst_addr;
  cthread->events[cthread->num_events].type = WQEVENT_DATA_WRITE;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->num_events++;
}
#else
void trace_data_write(void* cthread, unsigned int size, uint64_t dst_frame_addr)
{
}
#endif
