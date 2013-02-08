#include <inttypes.h>
#include <time.h>

#include "trace.h"
#include "wstream_df.h"
#include "arch.h"

static const char* state_names[] = {
  "seeking",
  "taskexec",
  "tcreate",
  "resdep",
  "tdec",
  "broadcast"
};

void trace_event(wstream_df_thread_p cthread, unsigned int type)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc();
  cthread->events[cthread->num_events].type = type;
  cthread->num_events++;
}

void trace_task_exec_start(wstream_df_thread_p cthread, unsigned int from_node, unsigned int type)
{
  assert(cthread->num_events < MAX_WQEVENT_SAMPLES-1);
  cthread->events[cthread->num_events].time = rdtsc();
  cthread->events[cthread->num_events].texec.from_node = from_node;
  cthread->events[cthread->num_events].texec.type = type;
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

  printf("Overall average parallelism: %.6f\n",
	 (double)parallelism_time / (double)(max_time - min_time));

  fclose(fp);
}

void dump_average_task_durations(int num_workers, wstream_df_thread_p wstream_df_worker_threads)
{
  uint64_t task_durations[TASK_DURATION_MAX];
  uint64_t num_tasks[TASK_DURATION_MAX];
  uint64_t duration;
  wstream_df_thread_p th;
  unsigned int i;
  int start_idx, end_idx;
  int type_idx;
  uint64_t total_num_tasks = 0;
  uint64_t total_duration = 0;

  memset(task_durations, 0, sizeof(task_durations));
  memset(num_tasks, 0, sizeof(num_tasks));

  for (i = 0; i < (unsigned int)num_workers; ++i) {
    th = &wstream_df_worker_threads[i];
    end_idx = 0;

    if(th->num_events > 0) {
      while((start_idx = get_next_event(th, end_idx, WQEVENT_START_TASKEXEC)) != -1 && end_idx != -1) {
	end_idx = get_next_event(th, start_idx, WQEVENT_END_TASKEXEC);

	if(end_idx != -1) {
	  assert(th->events[end_idx].time > th->events[start_idx].time);

	  duration = th->events[end_idx].time - th->events[start_idx].time;

	  if(th->events[start_idx].texec.type == STEAL_TYPE_PUSH) {
	    if(th->worker_id == th->events[start_idx].texec.from_node)
	      type_idx = TASK_DURATION_PUSH_SAMEL1;
	    else if(th->worker_id / 2 == th->events[start_idx].texec.from_node / 2)
	      type_idx = TASK_DURATION_PUSH_SAMEL2;
	    else if(th->worker_id / 8 == th->events[start_idx].texec.from_node / 8)
	      type_idx = TASK_DURATION_PUSH_SAMEL3;
	    else
	      type_idx = TASK_DURATION_PUSH_REMOTE;
	  } else if(th->events[start_idx].texec.type == STEAL_TYPE_STEAL) {
	    if(th->worker_id / 2 == th->events[start_idx].texec.from_node / 2)
	      type_idx = TASK_DURATION_STEAL_SAMEL2;
	    else if(th->worker_id / 8 == th->events[start_idx].texec.from_node / 8)
	      type_idx = TASK_DURATION_STEAL_SAMEL3;
	    else
	      type_idx = TASK_DURATION_STEAL_REMOTE;
	  } else {
	    assert(0);
	  }

	  task_durations[type_idx] += duration;
	  num_tasks[type_idx]++;

	  total_duration += duration;
	  total_num_tasks++;
	}
      }
    }
  }

  printf("Overall task num / duration (push, same L1): "
	 "%"PRIu64" / %"PRIu64" cycles (%.6Lf%% / %.6Lf%%), "
	 "%"PRIu64" cycles / task\n",
	 num_tasks[TASK_DURATION_PUSH_SAMEL1], task_durations[TASK_DURATION_PUSH_SAMEL1],
	 100.0*(long double)num_tasks[TASK_DURATION_PUSH_SAMEL1]/(long double)total_num_tasks,
	 100.0*(long double)task_durations[TASK_DURATION_PUSH_SAMEL1]/(long double)total_duration,
	 num_tasks[TASK_DURATION_PUSH_SAMEL1] == 0 ? 0 : task_durations[TASK_DURATION_PUSH_SAMEL1] / num_tasks[TASK_DURATION_PUSH_SAMEL1]);

  printf("Overall task num / duration (push, same L2): "
	 "%"PRIu64" / %"PRIu64" cycles (%.6Lf%% / %.6Lf%%) "
	 "%"PRIu64" cycles / task\n",
	 num_tasks[TASK_DURATION_PUSH_SAMEL2], task_durations[TASK_DURATION_PUSH_SAMEL2],
	 100.0*(long double)num_tasks[TASK_DURATION_PUSH_SAMEL2]/(long double)total_num_tasks,
	 100.0*(long double)task_durations[TASK_DURATION_PUSH_SAMEL2]/(long double)total_duration,
	 num_tasks[TASK_DURATION_PUSH_SAMEL2] == 0 ? 0 : task_durations[TASK_DURATION_PUSH_SAMEL2] / num_tasks[TASK_DURATION_PUSH_SAMEL2]);

  printf("Overall task num / duration (push, same L3): "
	 "%"PRIu64" / %"PRIu64" cycles (%.6Lf%% / %.6Lf%%) "
	 "%"PRIu64" cycles / task\n",
	 num_tasks[TASK_DURATION_PUSH_SAMEL3], task_durations[TASK_DURATION_PUSH_SAMEL3],
	 100.0*(long double)num_tasks[TASK_DURATION_PUSH_SAMEL3]/(long double)total_num_tasks,
	 100.0*(long double)task_durations[TASK_DURATION_PUSH_SAMEL3]/(long double)total_duration,
	 num_tasks[TASK_DURATION_PUSH_SAMEL3] == 0 ? 0 : task_durations[TASK_DURATION_PUSH_SAMEL3] / num_tasks[TASK_DURATION_PUSH_SAMEL3]);

  printf("Overall task num / duration (push, remote): "
	 "%"PRIu64" / %"PRIu64" cycles (%.6Lf%% / %.6Lf%%) "
	 "%"PRIu64" cycles / task\n",
	 num_tasks[TASK_DURATION_PUSH_REMOTE], task_durations[TASK_DURATION_PUSH_REMOTE],
	 100.0*(long double)num_tasks[TASK_DURATION_PUSH_REMOTE]/(long double)total_num_tasks,
	 100.0*(long double)task_durations[TASK_DURATION_PUSH_REMOTE]/(long double)total_duration,
	 num_tasks[TASK_DURATION_PUSH_REMOTE] == 0 ? 0 : task_durations[TASK_DURATION_PUSH_REMOTE] / num_tasks[TASK_DURATION_PUSH_REMOTE]);

  printf("Overall task num / duration (steal, same L2): "
	 "%"PRIu64" / %"PRIu64" cycles (%.6Lf%% / %.6Lf%%) "
	 "%"PRIu64" cycles / task\n",
	 num_tasks[TASK_DURATION_STEAL_SAMEL2], task_durations[TASK_DURATION_STEAL_SAMEL2],
	 100.0*(long double)num_tasks[TASK_DURATION_STEAL_SAMEL2]/(long double)total_num_tasks,
	 100.0*(long double)task_durations[TASK_DURATION_STEAL_SAMEL2]/(long double)total_duration,
	 num_tasks[TASK_DURATION_STEAL_SAMEL2] == 0 ? 0 : task_durations[TASK_DURATION_STEAL_SAMEL2] / num_tasks[TASK_DURATION_STEAL_SAMEL2]);

  printf("Overall task num / duration (steal, same L3): "
	 "%"PRIu64" / %"PRIu64" cycles (%.6Lf%% / %.6Lf%%) "
	 "%"PRIu64" cycles / task\n",
	 num_tasks[TASK_DURATION_STEAL_SAMEL3], task_durations[TASK_DURATION_STEAL_SAMEL3],
	 100.0*(long double)num_tasks[TASK_DURATION_STEAL_SAMEL3]/(long double)total_num_tasks,
	 100.0*(long double)task_durations[TASK_DURATION_STEAL_SAMEL3]/(long double)total_duration,
	 num_tasks[TASK_DURATION_STEAL_SAMEL3] == 0 ? 0 : task_durations[TASK_DURATION_STEAL_SAMEL3] / num_tasks[TASK_DURATION_STEAL_SAMEL3]);

  printf("Overall task num / duration (steal, remote): "
	 "%"PRIu64" / %"PRIu64" cycles (%.6Lf%% / %.6Lf%%) "
	 "%"PRIu64" cycles / task\n",
	 num_tasks[TASK_DURATION_STEAL_REMOTE], task_durations[TASK_DURATION_STEAL_REMOTE],
	 100.0*(long double)num_tasks[TASK_DURATION_STEAL_REMOTE]/(long double)total_num_tasks,
	 100.0*(long double)task_durations[TASK_DURATION_STEAL_REMOTE]/(long double)total_duration,
	 num_tasks[TASK_DURATION_STEAL_REMOTE] == 0 ? 0 : task_durations[TASK_DURATION_STEAL_REMOTE] / num_tasks[TASK_DURATION_STEAL_REMOTE]);
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

    if(th->num_events > 0) {
      for(k = 0; k < th->num_events-1; k++) {
	/* States */
	if(th->events[k].type == WQEVENT_STATECHANGE) {
	  if(last_state_idx != -1) {
	    state = th->events[last_state_idx].state_change.state;

	    /* Not the first state change, so using last_state_idx is safe */
	    fprintf(fp, "1:%d:1:1:%d:%"PRIu64":%"PRIu64":%d\n",
		    (th->worker_id+1),
		    (th->worker_id+1),
		    th->events[last_state_idx].time-min_time,
		    th->events[k].time-min_time,
		    state);

	    state_durations[state] += th->events[k].time - th->events[last_state_idx].time;
	    total_duration += th->events[k].time - th->events[last_state_idx].time;
	  } else {
	    /* First state change, by default the initial state is "seeking" */
	    fprintf(fp, "1:%d:1:1:%d:%"PRIu64":%"PRIu64":%d\n",
		    (th->worker_id+1),
		    (th->worker_id+1),
		    (uint64_t)0,
		    th->events[k].time-min_time,
		    WORKER_STATE_SEEKING);

	    state_durations[WORKER_STATE_SEEKING] += th->events[k].time-min_time;
	    total_duration += th->events[k].time-min_time;
	  }

	  last_state_idx = k;
	} else if(th->events[k].type == WQEVENT_STEAL) {
	  /* Steal events (dumped as communication) */
	  fprintf(fp, "3:%d:1:1:%d:%"PRIu64":%"PRIu64":%d:1:1:%d:%"PRIu64":%"PRIu64":%d:1\n",
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
	  fprintf(fp, "3:%d:1:1:%d:%"PRIu64":%"PRIu64":%d:1:1:%d:%"PRIu64":%"PRIu64":%d:1\n",
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
	  fprintf(fp, "2:%d:1:1:%d:%"PRIu64":%d:1\n",
		  (th->worker_id+1),
		  (th->worker_id+1),
		  th->events[k].time-min_time,
		  WQEVENT_TCREATE);
	}
      }

      /* Final state is "seeking" (beginning at the last state,
       * finishing at program termination) */
      fprintf(fp, "1:%d:1:1:%d:%"PRIu64":%"PRIu64":%d\n",
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
