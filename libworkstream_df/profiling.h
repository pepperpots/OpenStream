#ifndef PROFILING_H
#define PROFILING_H

#include "config.h"
#include "wstream_df.h"

#ifdef WQUEUE_PROFILE
void
init_wqueue_counters (wstream_df_thread_p th);

void
dump_wqueue_counters (wstream_df_thread_p th);

#define inc_wqueue_counter(ctr, delta) \
	do { \
	(*(ctr)) += delta; \
	} while (0)

#else
#define init_wqueue_counters(th) do {} while(0)
#define dump_wqueue_counters(th) do {} while(0)
#define inc_wqueue_counter(ctr, delta) do {} while(0)
#endif

#endif
