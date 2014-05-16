static inline void trace_state_change(wstream_df_thread_p cthread, unsigned int state)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  assert(cthread->tsc_offset_init);

  int64_t this_rdtsc = rdtsc();

  cthread->events[cthread->num_events].time = this_rdtsc - cthread->tsc_offset;
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

static inline void trace_state_restore(wstream_df_thread_p cthread)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  assert(cthread->tsc_offset_init);

  int64_t this_rdtsc = rdtsc();

  cthread->events[cthread->num_events].time = this_rdtsc - cthread->tsc_offset;
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

static inline void trace_event(wstream_df_thread_p cthread, unsigned int type)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  assert(cthread->tsc_offset_init);

  int64_t this_rdtsc = rdtsc();

  cthread->events[cthread->num_events].time = this_rdtsc - cthread->tsc_offset;
  cthread->events[cthread->num_events].type = type;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->num_events++;
}

static inline void trace_update_tcreate_fp(struct wstream_df_thread* cthread, struct wstream_df_frame* frame)
{
  cthread->last_tcreate_event->tcreate.frame = (uint64_t)frame;
  cthread->last_tcreate_event = NULL;
}

static inline void trace_tcreate(struct wstream_df_thread* cthread, struct wstream_df_frame* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  assert(cthread->tsc_offset_init);

  int64_t this_rdtsc = rdtsc();

  cthread->events[cthread->num_events].time = this_rdtsc - cthread->tsc_offset;
  cthread->events[cthread->num_events].type = WQEVENT_TCREATE;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->events[cthread->num_events].tcreate.frame = (uint64_t)frame;
  cthread->last_tcreate_event = &cthread->events[cthread->num_events];
  cthread->num_events++;
}

static inline void trace_steal(wstream_df_thread_p cthread, unsigned int src_worker, unsigned int src_cpu, unsigned int size, void* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  assert(cthread->tsc_offset_init);

  int64_t this_rdtsc = rdtsc();

  cthread->events[cthread->num_events].time = this_rdtsc - cthread->tsc_offset;
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

static inline void trace_push(wstream_df_thread_p cthread, unsigned int dst_worker, unsigned int dst_cpu, unsigned int size, void* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  assert(cthread->tsc_offset_init);

  int64_t this_rdtsc = rdtsc();

  cthread->events[cthread->num_events].time = this_rdtsc - cthread->tsc_offset;
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

static inline void trace_data_read(struct wstream_df_thread* cthread, unsigned int src_cpu, unsigned int size, long long prod_ts, void* src_addr)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  assert(cthread->tsc_offset_init);

  int64_t this_rdtsc = rdtsc();

  cthread->events[cthread->num_events].time = this_rdtsc - cthread->tsc_offset;
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

static inline void trace_data_write(struct wstream_df_thread* cthread, unsigned int size, uint64_t dst_addr)
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

static inline void trace_counter_timestamp(struct wstream_df_thread* cthread, uint64_t counter_id, int64_t value, int64_t timestamp)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  assert(cthread->tsc_offset_init);

  int64_t this_rdtsc = timestamp;

  cthread->events[cthread->num_events].time = this_rdtsc - cthread->tsc_offset;
  cthread->events[cthread->num_events].counter.counter_id = counter_id;
  cthread->events[cthread->num_events].counter.value = value;
  cthread->events[cthread->num_events].type = WQEVENT_COUNTER;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->num_events++;
}

static inline void trace_counter(struct wstream_df_thread* cthread, uint64_t counter_id, int64_t value)
{
  trace_counter_timestamp(cthread, counter_id, value, rdtsc());
}

static inline void trace_measure(struct wstream_df_thread* cthread, int type)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  assert(cthread->tsc_offset_init);

  int64_t this_rdtsc = rdtsc();

  cthread->events[cthread->num_events].time = this_rdtsc - cthread->tsc_offset;
  cthread->events[cthread->num_events].type = type;
  cthread->num_events++;
}

static inline void trace_measure_start(struct wstream_df_thread* cthread)

{
  trace_measure(cthread, WQEVENT_MEASURE_START);
}

static inline void trace_measure_end(struct wstream_df_thread* cthread)
{
  trace_measure(cthread, WQEVENT_MEASURE_END);
}

static inline void trace_tdestroy(struct wstream_df_thread* cthread, struct wstream_df_frame* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  assert(cthread->tsc_offset_init);

  int64_t this_rdtsc = rdtsc();

  cthread->events[cthread->num_events].time = this_rdtsc - cthread->tsc_offset;
  cthread->events[cthread->num_events].type = WQEVENT_TDESTROY;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->events[cthread->num_events].tdestroy.frame = (uint64_t)frame;
  cthread->num_events++;
}

static inline void trace_task_exec_start(wstream_df_thread_p cthread, struct wstream_df_frame* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  assert(cthread->tsc_offset_init);

  int64_t this_rdtsc = rdtsc();

  cthread->events[cthread->num_events].time = this_rdtsc - cthread->tsc_offset;
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

static inline void trace_task_exec_end(wstream_df_thread_p cthread, struct wstream_df_frame* frame)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  assert(cthread->tsc_offset_init);

  int64_t this_rdtsc = rdtsc();

  cthread->events[cthread->num_events].time = this_rdtsc - cthread->tsc_offset;
  cthread->events[cthread->num_events].type = WQEVENT_END_TASKEXEC;
  cthread->events[cthread->num_events].cpu = cthread->cpu;
  cthread->events[cthread->num_events].active_task = (uint64_t)cthread->current_work_fn;
  cthread->events[cthread->num_events].active_frame = (uint64_t)cthread->current_frame;
  cthread->events[cthread->num_events].texec_end.frame = (uint64_t)frame;
  cthread->num_events++;
}
