#if !USE_STDATOMIC
#include "cdeque-native.c.h"
#else
#include "cdeque-c11.c.h"
#endif
