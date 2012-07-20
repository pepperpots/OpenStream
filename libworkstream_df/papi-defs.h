#ifndef PAPI_DEFS_H
#define PAPI_DEFS_H

#ifdef _PAPI_PROFILE
#include <papi.h>
#endif

#define _PAPI_COUNTER_SETS 3

#ifndef _PAPI_PROFILE
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
# define _PAPI_P0B start_papi_counters (0)
# define _PAPI_P0E stop_papi_counters (0)
# define _PAPI_P1B start_papi_counters (1)
# define _PAPI_P1E stop_papi_counters (1)
# define _PAPI_P2B start_papi_counters (2)
# define _PAPI_P2E stop_papi_counters (2)
//# define _PAPI_P3B start_papi_counters (3)
//# define _PAPI_P3E stop_papi_counters (3)

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
