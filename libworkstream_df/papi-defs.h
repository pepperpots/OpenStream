#ifndef PAPI_DEFS_H
#define PAPI_DEFS_H

#include "config.h"

#ifdef _PAPI_PROFILE
#include <papi.h>
#endif

#define _PAPI_COUNTER_SETS 3

#ifndef _PAPI_PROFILE
#define WSTREAM_DF_THREAD_PAPI_FIELDS
# define _PAPI_P0B
# define _PAPI_P0E
# define _PAPI_P1B
# define _PAPI_P1E
# define _PAPI_P2B
# define _PAPI_P2E
# define _PAPI_P3B
# define _PAPI_P3E

# define _PAPI_INIT_CTRS(I)
# define _PAPI_DUMP_CTRS(I)
#else

#define WSTREAM_DF_THREAD_PAPI_FIELDS \
	int _papi_eset[16];	      \
	long long counters[16][_papi_num_events]

# define _papi_num_events 4
extern int _papi_tracked_events[_papi_num_events];

void dump_papi_counters (int);
void init_papi_counters (int);
void start_papi_counters (int);
void stop_papi_counters (int);
void accum_papi_counters (int);

# define _PAPI_P0B start_papi_counters (0)
# define _PAPI_P0E stop_papi_counters (0)
# define _PAPI_P1B start_papi_counters (1)
# define _PAPI_P1E stop_papi_counters (1)
# define _PAPI_P2B start_papi_counters (2)
# define _PAPI_P2E stop_papi_counters (2)
# define _PAPI_P3B start_papi_counters (3)
# define _PAPI_P3E stop_papi_counters (3)

# define _PAPI_INIT_CTRS(I)			\
  {						\
    int _ii;					\
    for (_ii = 0; _ii < I; ++ _ii)		\
      init_papi_counters (_ii);			\
  }

# define _PAPI_DUMP_CTRS(I)			\
  {						\
    int _ii;					\
    for (_ii = 0; _ii < I; ++ _ii)		\
      dump_papi_counters (_ii);			\
  }
#endif

#endif
