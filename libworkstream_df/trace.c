#include <inttypes.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <string.h>

#include "trace.h"
#include "wstream_df.h"
#include "arch.h"

#if ALLOW_WQEVENT_SAMPLING
static const char* state_names[] = {
  "seeking",
  "taskexec",
  "tcreate",
  "resdep",
  "tdec",
  "broadcast",
  "init",
  "estimate_costs"
};

void trace_init(struct wstream_df_thread* cthread)
{
	cthread->num_events = 0;
	cthread->previous_state_idx = 0;
}

void trace_event(wstream_df_thread_p cthread, unsigned int type)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc();
  cthread->events[cthread->num_events].type = type;
  cthread->num_events++;
}

void trace_task_exec_start(wstream_df_thread_p cthread, unsigned int from_node, unsigned int type,
			   uint64_t creation_timestamp, uint64_t ready_timestamp, uint32_t size,
			   uint64_t cache_misses, uint64_t allocator_cache_misses)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc();
  cthread->events[cthread->num_events].texec.from_node = from_node;
  cthread->events[cthread->num_events].texec.type = type;
  cthread->events[cthread->num_events].texec.creation_timestamp = creation_timestamp;
  cthread->events[cthread->num_events].texec.ready_timestamp = ready_timestamp;
  cthread->events[cthread->num_events].texec.size = size;
  cthread->events[cthread->num_events].texec.cache_misses = cache_misses;
  cthread->events[cthread->num_events].texec.allocator_cache_misses = allocator_cache_misses;
  cthread->events[cthread->num_events].type = WQEVENT_START_TASKEXEC;
  cthread->previous_state_idx = cthread->num_events;
  cthread->num_events++;
}

void trace_task_exec_end(wstream_df_thread_p cthread)
{
  trace_event(cthread, WQEVENT_END_TASKEXEC);
}

void trace_state_change(wstream_df_thread_p cthread, unsigned int state)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc();
  cthread->events[cthread->num_events].state_change.state = state;
  cthread->events[cthread->num_events].type = WQEVENT_STATECHANGE;

  cthread->events[cthread->num_events].state_change.previous_state_idx =
    cthread->previous_state_idx;

  cthread->previous_state_idx = cthread->num_events;
  cthread->num_events++;
}

void trace_state_restore(wstream_df_thread_p cthread)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc();
  cthread->events[cthread->num_events].type = WQEVENT_STATECHANGE;

  cthread->events[cthread->num_events].state_change.state =
    cthread->events[cthread->previous_state_idx].state_change.state;

  cthread->events[cthread->num_events].state_change.previous_state_idx =
    cthread->events[cthread->previous_state_idx].state_change.previous_state_idx;

  cthread->previous_state_idx = cthread->num_events;
  cthread->num_events++;
}

void trace_steal(wstream_df_thread_p cthread, unsigned int src, unsigned int size)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc();
  cthread->events[cthread->num_events].steal.src = src;
  cthread->events[cthread->num_events].steal.size = size;
  cthread->events[cthread->num_events].type = WQEVENT_STEAL;
  cthread->num_events++;
}

void trace_push(wstream_df_thread_p cthread, unsigned int dst, unsigned int size)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc();
  cthread->events[cthread->num_events].push.dst = dst;
  cthread->events[cthread->num_events].push.size = size;
  cthread->events[cthread->num_events].type = WQEVENT_PUSH;
  cthread->num_events++;
}

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

int64_t get_min_time(int num_workers, wstream_df_thread_p wstream_df_worker_threads)
{
  int i;
  int64_t min = -1;

  for(i = 0; i < num_workers; i++)
    if(wstream_df_worker_threads[i].num_events > 0)
      if(min == -1 || wstream_df_worker_threads[i].events[0].time < (uint64_t)min)
	min = wstream_df_worker_threads[i].events[0].time;

  return min;
}

int64_t get_max_time(int num_workers, wstream_df_thread_p wstream_df_worker_threads)
{
  int i;
  int64_t max = -1;

  for(i = 0; i < num_workers; i++)
    if(wstream_df_worker_threads[i].num_events > 0)
      if(max == -1 || wstream_df_worker_threads[i].events[wstream_df_worker_threads[i].num_events-1].time > (uint64_t)max)
	max = wstream_df_worker_threads[i].events[wstream_df_worker_threads[i].num_events-1].time;

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
  int i, idx_start, idx_end, idx, idx_tmp;
  wstream_df_thread_p th;

  *max_duration = 0;
  *num_tasks = 0;
  *total_duration = 0;
  *max_ready_to_exec_time = 0;
  *max_create_to_exec_time = 0;
  *max_create_to_ready_time = 0;

  for(i = 0; i < num_workers; i++) {
    idx = -1;
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

void dump_task_duration_histogram_worker(int worker_start, int worker_end,
					 wstream_df_thread_p wstream_df_worker_threads,
					 uint64_t total_duration, uint64_t num_tasks,
					 uint64_t max_duration)
{
  uint64_t duration;
  int i, idx_start, idx_end, idx_hist, idx_tmp;
  wstream_df_thread_p th;
  uint64_t histogram[NUM_WQEVENT_TASKHIST_BINS][MEM_NUM_LEVELS][2];
  uint64_t histogram_all[NUM_WQEVENT_TASKHIST_BINS];
  uint64_t histogram_dur[NUM_WQEVENT_TASKHIST_BINS][MEM_NUM_LEVELS][2];
  uint64_t histogram_dur_all[NUM_WQEVENT_TASKHIST_BINS];
  uint64_t histogram_ctet[NUM_WQEVENT_TASKHIST_BINS][MEM_NUM_LEVELS][2];
  uint64_t histogram_ctrt[NUM_WQEVENT_TASKHIST_BINS][MEM_NUM_LEVELS][2];
  uint64_t histogram_rtet[NUM_WQEVENT_TASKHIST_BINS][MEM_NUM_LEVELS][2];
  uint64_t histogram_size[NUM_WQEVENT_TASKHIST_BINS][MEM_NUM_LEVELS][2];
  uint64_t histogram_otherstates[NUM_WQEVENT_TASKHIST_BINS][MEM_NUM_LEVELS][2];
  uint64_t histogram_misses[NUM_WQEVENT_TASKHIST_BINS][MEM_NUM_LEVELS][2];
  uint64_t histogram_allocator_misses[NUM_WQEVENT_TASKHIST_BINS][MEM_NUM_LEVELS][2];
  uint64_t curr_bin;
  int64_t state_duration;
  int64_t duration_other_states;
  FILE* fp;
  int common_level;
  int steal_type;
  int level;

  uint64_t create_to_exec_time;
  uint64_t create_to_ready_time;
  uint64_t ready_to_exec_time;

  memset(histogram, 0, sizeof(histogram));
  memset(histogram_all, 0, sizeof(histogram_all));
  memset(histogram_dur, 0, sizeof(histogram_dur));
  memset(histogram_dur_all, 0, sizeof(histogram_dur_all));
  memset(histogram_ctet, 0, sizeof(histogram_ctet));
  memset(histogram_ctrt, 0, sizeof(histogram_ctrt));
  memset(histogram_rtet, 0, sizeof(histogram_rtet));
  memset(histogram_size, 0, sizeof(histogram_size));
  memset(histogram_otherstates, 0, sizeof(histogram_otherstates));
  memset(histogram_misses, 0, sizeof(histogram_misses));
  memset(histogram_allocator_misses, 0, sizeof(histogram_allocator_misses));

  for(i = worker_start; i < worker_end; i++) {
    idx_start = -1;

    th = &wstream_df_worker_threads[i];

    while((idx_start = get_next_event(th, idx_start, WQEVENT_START_TASKEXEC)) != -1) {
      idx_end = get_next_event(th, idx_start, WQEVENT_END_TASKEXEC);
      if(idx_end != -1) {
	common_level = mem_lowest_common_level(i, th->events[idx_start].texec.from_node);
	steal_type = th->events[idx_start].texec.type;
	assert(th->events[idx_start].texec.type != STEAL_TYPE_UNKNOWN);

	idx_tmp = idx_start+1;
	duration = 0;
	while((state_duration = get_state_duration(th, &idx_tmp, idx_end)) != -1)
	  duration += state_duration;

	duration_other_states = th->events[idx_end].time - th->events[idx_start].time - duration;

	idx_hist = (duration*(NUM_WQEVENT_TASKHIST_BINS-1)) / max_duration;
	histogram[idx_hist][common_level][steal_type]++;
	histogram_dur[idx_hist][common_level][steal_type] += duration;

	create_to_exec_time = th->events[idx_start].time - th->events[idx_start].texec.creation_timestamp;
	create_to_ready_time = th->events[idx_start].texec.ready_timestamp - th->events[idx_start].texec.creation_timestamp;
	ready_to_exec_time = th->events[idx_start].time - th->events[idx_start].texec.ready_timestamp;

	histogram_ctet[idx_hist][common_level][steal_type] += create_to_exec_time;
	histogram_rtet[idx_hist][common_level][steal_type] += ready_to_exec_time;
	histogram_ctrt[idx_hist][common_level][steal_type] += create_to_ready_time;
	histogram_size[idx_hist][common_level][steal_type] += th->events[idx_start].texec.size;
	histogram_otherstates[idx_hist][common_level][steal_type] += duration_other_states;
	histogram_misses[idx_hist][common_level][steal_type] += th->events[idx_start].texec.cache_misses;
	histogram_allocator_misses[idx_hist][common_level][steal_type] += th->events[idx_start].texec.allocator_cache_misses;

	idx_start = idx_end;
      }
    }
  }

  fp = fopen(WQEVENT_SAMPLING_TASKHISTFILE, "w+");
  assert(fp != NULL);

  i = 1;
  fprintf(fp, "# %d: task length\n", i);
  for(level = 0; level < MEM_NUM_LEVELS; level++) {
      for(steal_type = 0; steal_type < 2; steal_type++) {
	i++;
	fprintf(fp, "# %d: Number of tasks (%s, %s)\n",
		i, mem_level_name(level),
		steal_type_str(steal_type));

	i++;
	fprintf(fp, "# %d: Number of tasks [%%] (%s, %s)\n",
		i, mem_level_name(level),
		steal_type_str(steal_type));

	i++;
	fprintf(fp, "# %d: Duration [cycles] (%s, %s)\n",
		i, mem_level_name(level),
		steal_type_str(steal_type));
	i++;
	fprintf(fp, "# %d: Duration [%%] (%s, %s)\n",
		i, mem_level_name(level),
		steal_type_str(steal_type));

	i++;
	fprintf(fp, "# %d: Duration, other states than Texec [cycles] (%s, %s)\n",
		i, mem_level_name(level),
		steal_type_str(steal_type));
	i++;
	fprintf(fp, "# %d: Duration per task, other states than Texec [cycles] (%s, %s)\n",
		i, mem_level_name(level),
		steal_type_str(steal_type));

	i++;
	fprintf(fp, "# %d: Average create to exec time [cycles] (%s, %s)\n",
		i, mem_level_name(level),
		steal_type_str(steal_type));

	i++;
	fprintf(fp, "# %d: Average ready to exec time [cycles] (%s, %s)\n",
		i, mem_level_name(level),
		steal_type_str(steal_type));

	i++;
	fprintf(fp, "# %d: Average create to ready time [cycles] (%s, %s)\n",
		i, mem_level_name(level),
		steal_type_str(steal_type));

	i++;
	fprintf(fp, "# %d: Average frame size [bytes] (%s, %s)\n",
		i, mem_level_name(level),
		steal_type_str(steal_type));

	i++;
	fprintf(fp, "# %d: Misses (sum) on writing nodes since first write (%s, %s)\n",
		i, mem_level_name(level),
		steal_type_str(steal_type));

	i++;
	fprintf(fp, "# %d: Misses on allocating node since task creation (%s, %s)\n",
		i, mem_level_name(level),
		steal_type_str(steal_type));
      }
  }

  for(i = 0; i < NUM_WQEVENT_TASKHIST_BINS; i++) {
    curr_bin = (max_duration*i) / NUM_WQEVENT_TASKHIST_BINS;

    fprintf(fp, "%"PRIu64" ", curr_bin);

    for(level = 0; level < MEM_NUM_LEVELS; level++) {
      for(steal_type = 0; steal_type < 2; steal_type++) {
	fprintf(fp, "%"PRIu64" %Lf %"PRIu64" %Lf %"PRIu64" %Lf %Lf %Lf %Lf %Lf %Lf %Lf ",
		histogram[i][level][steal_type],
		100.0 * ((long double)histogram[i][level][steal_type]) / ((long double)num_tasks),
		histogram_dur[i][level][steal_type],
		100.0 * ((long double)histogram_dur[i][level][steal_type]) / ((long double)total_duration),
		histogram_otherstates[i][level][steal_type],
		histogram[i][level][steal_type] != 0 ? ((long double)histogram_otherstates[i][level][steal_type]) / ((long double)histogram[i][level][steal_type]) : 0.0,
		histogram[i][level][steal_type] != 0 ? ((long double)histogram_ctet[i][level][steal_type]) / ((long double)histogram[i][level][steal_type]) : 0.0,
		histogram[i][level][steal_type] != 0 ? ((long double)histogram_rtet[i][level][steal_type]) / ((long double)histogram[i][level][steal_type]) : 0.0,
		histogram[i][level][steal_type] != 0 ? ((long double)histogram_ctrt[i][level][steal_type]) / ((long double)histogram[i][level][steal_type]) : 0.0,
		histogram[i][level][steal_type] != 0 ? ((long double)histogram_size[i][level][steal_type]) / ((long double)histogram[i][level][steal_type]) : 0.0,
		histogram[i][level][steal_type] != 0 ? ((long double)histogram_misses[i][level][steal_type]) / ((long double)histogram[i][level][steal_type]) : 0.0,
		histogram[i][level][steal_type] != 0 ? ((long double)histogram_allocator_misses[i][level][steal_type]) / ((long double)histogram[i][level][steal_type]) : 0.0);
      }
    }

    fprintf(fp, "\n");
  }

  fclose(fp);
}

void dump_task_duration_histogram(int num_workers, wstream_df_thread_p wstream_df_worker_threads)
{
  uint64_t total_duration = 0;
  uint64_t num_tasks = 0;
  uint64_t max_duration = 0;
  uint64_t max_create_to_exec_time = 0;
  uint64_t max_create_to_ready_time = 0;
  uint64_t max_ready_to_exec_time = 0;

  get_task_duration_stats(num_workers, wstream_df_worker_threads,
			  &total_duration, &num_tasks, &max_duration,
			  &max_create_to_exec_time, &max_create_to_ready_time,
			  &max_ready_to_exec_time);

  dump_task_duration_histogram_worker(0, num_workers, wstream_df_worker_threads,
				      total_duration, num_tasks, max_duration);
}

void dump_avg_state_parallelism(unsigned int state, uint64_t max_intervals, int num_workers, wstream_df_thread_p wstream_df_worker_threads)
{
  int curr_idx[num_workers];
  unsigned int last_state[num_workers];
  int curr_parallelism = 0;
  double parallelism_time = 0.0;
  double parallelism_time_interval = 0.0;
  int i;
  uint64_t min_time = get_min_time(num_workers, wstream_df_worker_threads);
  uint64_t max_time = get_max_time(num_workers, wstream_df_worker_threads);
  uint64_t last_time = min_time;
  uint64_t last_time_interval = min_time;
  uint64_t interval_length = (max_time-min_time)/max_intervals;
  worker_state_change_p curr_event;
  uint64_t num_tasks_executed = 0;
  FILE* fp = fopen(WQEVENT_SAMPLING_PARFILE, "w+");

  assert(fp != NULL);

  memset(curr_idx, 0, num_workers*sizeof(curr_idx[0]));

  for(i = 0; i < num_workers; i++) {
    curr_idx[i] = get_next_state_change(&wstream_df_worker_threads[i], 0);
    last_state[i] = WORKER_STATE_SEEKING;
  }

  while((i = get_min_index(curr_idx, num_workers, wstream_df_worker_threads))!= -1) {
    curr_event = &wstream_df_worker_threads[i].events[curr_idx[i]];
    parallelism_time += (double)(curr_event->time - last_time) * curr_parallelism;
    parallelism_time_interval += (double)(curr_event->time - last_time) * curr_parallelism;

    if(curr_event->state_change.state == state)
      curr_parallelism++;
    else if(last_state[i] == state)
      curr_parallelism--;

    last_state[i] = curr_event->state_change.state;
    last_time = curr_event->time;
    curr_idx[i] = get_next_state_change(&wstream_df_worker_threads[i], curr_idx[i]);

    if(curr_event->time - last_time_interval > interval_length) {
      fprintf(fp, "%"PRIu64" %f\n", curr_event->time-min_time, (double)parallelism_time_interval /(double)(curr_event->time - last_time_interval));
      last_time_interval = curr_event->time;
      parallelism_time_interval = 0.0;
    }
  }

  printf("Overall average parallelism for state %s: %.6f\n",
	 state_names[state],
	 (double)parallelism_time / (double)(max_time - min_time));

#ifdef WQUEUE_PROFILE
  for(i = 0; i < num_workers; i++) {
    num_tasks_executed += wstream_df_worker_threads[i].tasks_executed;
  }

  printf("Overall state duration per task for state %s: %.6f\n",
	 state_names[state],
	 (double)parallelism_time / (double)num_tasks_executed);
#endif

  fclose(fp);
}

void dump_average_task_duration(unsigned int num_intervals, int num_workers, wstream_df_thread_p wstream_df_worker_threads)
{
  int curr_idx[num_workers];
  int end_idx, tmp_idx;
  int i;
  int curr_worker = -1;
  uint64_t min_texec_time;
  uint64_t duration = 0;
  int64_t texec_duration;
  uint64_t min_time = get_min_time(num_workers, wstream_df_worker_threads);
  uint64_t max_time = get_max_time(num_workers, wstream_df_worker_threads);
  uint64_t interval_length = (max_time - min_time) / num_intervals;
  uint64_t interval_end = min_time + interval_length;
  unsigned int tasks_in_interval = 0;
  wstream_df_thread_p th;
  FILE* fp = fopen(WQEVENT_SAMPLING_TASKLENGTHFILE, "w+");
  assert(fp != NULL);

  /* Initialize curr_idx with indexes of the first task executions */
  for(i = 0; i < num_workers; i++) {
    th = &wstream_df_worker_threads[i];
    curr_idx[i] = get_next_event(th, -1, WQEVENT_START_TASKEXEC);
  }

  do {
    min_texec_time = UINT64_MAX;
    curr_worker = -1;

    /* Find worker whose next task execution has the lowest timestamp */
    for(i = 0; i < num_workers; i++) {
      if(curr_idx[i] != -1) {
	th = &wstream_df_worker_threads[i];

	if(th->events[curr_idx[i]].time < min_texec_time) {
	  min_texec_time = th->events[curr_idx[i]].time;
	  curr_worker = i;
	}
      }
    }

    /* If there still is a worker executing a task, then process */
    if(curr_worker != -1) {
      th = &wstream_df_worker_threads[curr_worker];

      /* Find end of the task execution */
      end_idx = get_next_event(th, curr_idx[curr_worker], WQEVENT_END_TASKEXEC);

      if(end_idx != -1) {
	/* If we enter a new interval, dump information of the previous interval */
	if(th->events[curr_idx[curr_worker]].time > interval_end
	   && tasks_in_interval > 0)
	  {
	    fprintf(fp, "%"PRIu64" %"PRIu64"\n",
		    (interval_end - interval_length/2) - min_time,
		    duration/(uint64_t)tasks_in_interval);
	    interval_end += interval_length;
	    duration = 0;
	    tasks_in_interval = 0;
	  }

	/* Cumulate duration of texec states between start and end of the execution */
	tmp_idx = curr_idx[curr_worker];
	while((texec_duration = get_state_duration(th, &tmp_idx, end_idx)) != -1)
	    duration += texec_duration;

	/* Update worker's index */
	curr_idx[curr_worker] = get_next_event(th, end_idx, WQEVENT_START_TASKEXEC);
	tasks_in_interval++;
      } else {
	curr_idx[curr_worker] = -1;
      }
    }
  } while(curr_worker != -1);

  /* Dump last interval */
  if(tasks_in_interval > 0) {
    fprintf(fp, "%"PRIu64" %"PRIu64"\n",
	    (interval_end - interval_length/2) - min_time,
	    duration/(uint64_t)tasks_in_interval);
  }

  fclose(fp);
}

void dump_average_task_duration_summary(int num_workers, wstream_df_thread_p wstream_df_worker_threads)
{
  uint64_t task_durations_push[MEM_NUM_LEVELS];
  uint64_t task_durations_steal[MEM_NUM_LEVELS];
  uint64_t num_tasks_push[MEM_NUM_LEVELS];
  uint64_t num_tasks_steal[MEM_NUM_LEVELS];
  uint64_t duration;
  wstream_df_thread_p th;
  unsigned int i;
  int start_idx, end_idx, tmp_idx;
  int level;
  uint64_t total_num_tasks = 0;
  uint64_t total_duration = 0;
  int64_t state_duration;

  memset(task_durations_push, 0, sizeof(task_durations_push));
  memset(task_durations_steal, 0, sizeof(task_durations_steal));
  memset(num_tasks_push, 0, sizeof(num_tasks_push));
  memset(num_tasks_steal, 0, sizeof(num_tasks_steal));

  for (i = 0; i < (unsigned int)num_workers; ++i) {
    th = &wstream_df_worker_threads[i];
    end_idx = 0;

    if(th->num_events > 0) {
      while((start_idx = get_next_event(th, end_idx, WQEVENT_START_TASKEXEC)) != -1 && end_idx != -1) {
	end_idx = get_next_event(th, start_idx, WQEVENT_END_TASKEXEC);

	if(end_idx != -1) {
	  assert(th->events[end_idx].time > th->events[start_idx].time);

	  level = mem_lowest_common_level(th->worker_id, th->events[start_idx].texec.from_node);
	  duration = 0;
	  tmp_idx = start_idx+1;
	  while((state_duration = get_state_duration(th, &tmp_idx, end_idx)) != -1)
	    duration += state_duration;

	  if(th->events[start_idx].texec.type == STEAL_TYPE_PUSH) {
		  task_durations_push[level] += duration;
		  num_tasks_push[level]++;
	  } else if(th->events[start_idx].texec.type == STEAL_TYPE_STEAL) {
		  task_durations_steal[level] += duration;
		  num_tasks_steal[level]++;
	  } else {
	    assert(0);
	  }

	  total_duration += duration;
	  total_num_tasks++;
	}
      }
    }
  }

  for(level = 0; level < MEM_NUM_LEVELS; level++) {
	  printf("Overall task num / duration (push, %s): "
		 "%"PRIu64" / %"PRIu64" cycles (%.6Lf%% / %.6Lf%%), "
		 "%"PRIu64" cycles / task\n",
		 mem_level_name(level),
		 num_tasks_push[level], task_durations_push[level],
		 100.0*(long double)num_tasks_push[level]/(long double)total_num_tasks,
		 100.0*(long double)task_durations_push[level]/(long double)total_duration,
		 num_tasks_push[level] == 0 ? 0 : task_durations_push[level] / num_tasks_push[level]);

	  printf("Overall task num / duration (steal, %s): "
		 "%"PRIu64" / %"PRIu64" cycles (%.6Lf%% / %.6Lf%%), "
		 "%"PRIu64" cycles / task\n",
		 mem_level_name(level),
		 num_tasks_steal[level], task_durations_steal[level],
		 100.0*(long double)num_tasks_steal[level]/(long double)total_num_tasks,
		 100.0*(long double)task_durations_steal[level]/(long double)total_duration,
		 num_tasks_steal[level] == 0 ? 0 : task_durations_steal[level] / num_tasks_steal[level]);
  }

  printf("Overall average task duration: %"PRIu64" cycles / task\n", total_duration/total_num_tasks);
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
void dump_events(int num_workers, wstream_df_thread_p wstream_df_worker_threads)
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
  int do_dump;

  assert(fp != NULL);

  memset(state_durations, 0, sizeof(state_durations));

  assert(min_time != -1);
  assert(max_time != -1);

  /* Write paraver header */
  fprintf(fp, "#Paraver (%d/%d/%d at %d:%d):%"PRIu64":1(%d):1:1(%d:1)\n",
	  now->tm_mday,
	  now->tm_mon+1,
	  now->tm_year+1900,
	  now->tm_hour,
	  now->tm_min,
	  max_time-min_time,
	  num_workers,
	  num_workers);

  /* Dump events and states */
  for (i = 0; i < (unsigned int)num_workers; ++i) {
    th = &wstream_df_worker_threads[i];
    last_state_idx = -1;
    do_dump = 1;

    if(th->num_events > 0) {
      for(k = 0; k < th->num_events-1; k++) {
	if(MAX_WQEVENT_PARAVER_CYCLES != -1 &&
	   th->events[k].time-min_time > (int64_t)MAX_WQEVENT_PARAVER_CYCLES)
	  {
	    do_dump = 0;
	  }

	/* States */
	if(th->events[k].type == WQEVENT_STATECHANGE) {
	  if(last_state_idx != -1) {
	    state = th->events[last_state_idx].state_change.state;

	    /* Not the first state change, so using last_state_idx is safe */
	    conditional_fprintf(do_dump, fp, "1:%d:1:1:%d:%"PRIu64":%"PRIu64":%d\n",
		    (th->worker_id+1),
		    (th->worker_id+1),
		    th->events[last_state_idx].time-min_time,
		    th->events[k].time-min_time,
		    state);

	    state_durations[state] += th->events[k].time - th->events[last_state_idx].time;
	    total_duration += th->events[k].time - th->events[last_state_idx].time;
	  } else {
#ifdef TRACE_RT_INIT_STATE
	  /* First state change, by default the initial state is "initialization" */
	    conditional_fprintf(do_dump, fp, "1:%d:1:1:%d:%"PRIu64":%"PRIu64":%d\n",
		    (th->worker_id+1),
		    (th->worker_id+1),
		    (uint64_t)0,
		    th->events[k].time-min_time,
		    WORKER_STATE_RT_INIT);
#else
	    /* First state change, by default the initial state is "seeking" */
	    conditional_fprintf(do_dump, fp, "1:%d:1:1:%d:%"PRIu64":%"PRIu64":%d\n",
		    (th->worker_id+1),
		    (th->worker_id+1),
		    (uint64_t)0,
		    th->events[k].time-min_time,
		    WORKER_STATE_SEEKING);
#endif
	    state_durations[WORKER_STATE_SEEKING] += th->events[k].time-min_time;
	    total_duration += th->events[k].time-min_time;
	  }

	  last_state_idx = k;
	} else if(th->events[k].type == WQEVENT_STEAL) {
	  /* Steal events (dumped as communication) */
	  conditional_fprintf(do_dump, fp, "3:%d:1:1:%d:%"PRIu64":%"PRIu64":%d:1:1:%d:%"PRIu64":%"PRIu64":%d:1\n",
		  th->events[k].steal.src+1,
		  th->events[k].steal.src+1,
		  th->events[k].time-min_time,
		  th->events[k].time-min_time,
		  (th->worker_id+1),
		  (th->worker_id+1),
		  th->events[k].time-min_time,
		  th->events[k].time-min_time,
		  th->events[k].steal.size);
	} else if(th->events[k].type == WQEVENT_STEAL) {
	  /* Push events (dumped as communication) */
	  conditional_fprintf(do_dump, fp, "3:%d:1:1:%d:%"PRIu64":%"PRIu64":%d:1:1:%d:%"PRIu64":%"PRIu64":%d:1\n",
		  (th->worker_id+1),
		  (th->worker_id+1),
		  th->events[k].time-min_time,
		  th->events[k].time-min_time,
		  th->events[k].push.dst+1,
		  th->events[k].push.dst+1,
		  th->events[k].time-min_time,
		  th->events[k].time-min_time,
		  th->events[k].push.size);
	} else if(th->events[k].type == WQEVENT_TCREATE) {
	  /* Tcreate event (simply dumped as an event) */
	  conditional_fprintf(do_dump, fp, "2:%d:1:1:%d:%"PRIu64":%d:1\n",
		  (th->worker_id+1),
		  (th->worker_id+1),
		  th->events[k].time-min_time,
		  WQEVENT_TCREATE);
	}
      }

      /* Final state is "seeking" (beginning at the last state,
       * finishing at program termination) */
      conditional_fprintf(do_dump, fp, "1:%d:1:1:%d:%"PRIu64":%"PRIu64":%d\n",
	      (th->worker_id+1),
	      (th->worker_id+1),
	      th->events[last_state_idx].time-min_time,
	      max_time-min_time,
	      WORKER_STATE_SEEKING);

      state_durations[WORKER_STATE_SEEKING] += max_time-th->events[last_state_idx].time;
      total_duration += max_time-th->events[last_state_idx].time;
    }
  }

  for(i = 0; i < WORKER_STATE_MAX; i++) {
    printf("Overall time for state %s: %lld (%.6f %%)\n",
	   state_names[i],
	   state_durations[i],
	   ((double)state_durations[i] / (double)total_duration)*100.0);
  }

  fclose(fp);
}

#endif
