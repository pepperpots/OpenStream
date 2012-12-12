#ifndef COMMON_H
#define COMMON_H

#include <sys/time.h>
#include <stdlib.h>
#include <stdbool.h>

double tdiff (struct timeval *end, struct timeval *start);

static inline bool
double_equal (double a, double b)
{
  return (abs (a - b) < 1e-7);
}

#endif
