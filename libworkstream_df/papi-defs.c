#include "papi-defs.h"

#ifdef _PAPI_PROFILE
int _papi_tracked_events[_papi_num_events] =
  {PAPI_TOT_CYC, PAPI_L1_DCM, PAPI_L2_DCM, PAPI_L3_TCM};

void
dump_papi_counters (int eset_id)
{
  long long counters[_papi_num_events];
  char event_name[PAPI_MAX_STR_LEN];
  int i, pos = 0;
  int max_length = _papi_num_events * (PAPI_MAX_STR_LEN + 30);
  char out_buf[max_length];

  //PAPI_read (current_thread->_papi_eset[eset_id], counters);
  pos += sprintf (out_buf, "Thread %d (eset %d):", current_thread->worker_id, eset_id);
  for (i = 0; i < _papi_num_events; ++i)
    {
      PAPI_event_code_to_name (_papi_tracked_events[i], event_name);
      pos += sprintf (out_buf + pos, "\t %s %15lld", event_name, current_thread->counters[eset_id][i]);
    }
  printf ("%s\n", out_buf); fflush (stdout);
}

void
init_papi_counters (int eset_id)
{
  int retval, j;

  current_thread->_papi_eset[eset_id] = PAPI_NULL;
  if ((retval = PAPI_create_eventset (&current_thread->_papi_eset[eset_id])) != PAPI_OK)
    wstream_df_fatal ("Cannot create PAPI event set (%s)", PAPI_strerror (retval));

  if ((retval = PAPI_add_events (current_thread->_papi_eset[eset_id], _papi_tracked_events, _papi_num_events)) != PAPI_OK)
    wstream_df_fatal ("Cannot add events to set (%s)", PAPI_strerror (retval));

  for (j = 0; j < _papi_num_events; ++j)
    current_thread->counters[eset_id][j] = 0;
}


void
start_papi_counters (int eset_id)
{
  int retval;
  if ((retval = PAPI_start (current_thread->_papi_eset[eset_id])) != PAPI_OK)
    wstream_df_fatal ("Cannot sart PAPI counters (%s)", PAPI_strerror (retval));
}

void
stop_papi_counters (int eset_id)
{
  int retval, i;
  long long counters[_papi_num_events];

  if ((retval = PAPI_stop (current_thread->_papi_eset[eset_id], counters)) != PAPI_OK)
    wstream_df_fatal ("Cannot stop PAPI counters (%s)", PAPI_strerror (retval));

  for (i = 0; i < _papi_num_events; ++i)
    current_thread->counters[eset_id][i] += counters[i];
}

void
accum_papi_counters (int eset_id)
{
  int retval;

  if ((retval = PAPI_accum (current_thread->_papi_eset[eset_id], current_thread->counters[eset_id])) != PAPI_OK)
    wstream_df_fatal ("Cannot start PAPI counters (%s)", PAPI_strerror (retval));
}

#endif
