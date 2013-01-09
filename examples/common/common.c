#include "common.h"

double
tdiff (struct timeval *end, struct timeval *start)
{
  return (double)end->tv_sec - (double)start->tv_sec +
    (double)(end->tv_usec - start->tv_usec) / 1e6;
}
