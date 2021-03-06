#ifndef COMMON_H
#define COMMON_H

#include <sys/time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

double tdiff (struct timeval *end, struct timeval *start);

static inline int double_equal (double a, double b)
{
  if(a == 0.0)
    return (fabs (a - b) < 1e-5);
  else
    return (fabs (a - b) / fabs(a) < 5e-5);
}

static inline int int_min(int a, int b)
{
  if(a < b)
    return a;

  return b;
}

#endif
