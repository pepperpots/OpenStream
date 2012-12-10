//#define DEBUG 1
#define DEBUG_DUMP 1
#define INPUT_SIZE 2
#define TIMING_OUTPUT 1
#include <sys/time.h>

static inline double
tdiff (struct timeval *end, struct timeval *start)
{
  return (double)end->tv_sec - (double)start->tv_sec +
    (double)(end->tv_usec - start->tv_usec) / 1e6;
}

