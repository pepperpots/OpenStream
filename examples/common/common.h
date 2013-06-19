#ifndef COMMON_H
#define COMMON_H

#include <sys/time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

double tdiff (struct timeval *end, struct timeval *start);

static inline bool
double_equal (double a, double b)
{
  if(a == 0.0)
    return (fabs (a - b) < 1e-5);
  else
    return (fabs (a - b) / fabs(a) < 5e-5);
}

#endif
