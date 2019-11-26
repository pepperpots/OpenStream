#ifndef ARCH_H
#define ARCH_H

#include <stdint.h>
#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include "hwloc-support.h"

#define __compiler_fence __asm__ __volatile__ ("" ::: "memory")

/* Memory fences.  */
static inline void
load_load_fence (uintptr_t dep)
{
#if !NO_FENCES && defined(__arm__)
  /* Fences the load that produced DEP. */
  __asm__ __volatile__ ("teq %0, %0; beq 0f; 0: isb" :: "r" (dep) : "memory");
#else
  (void) dep;
  __compiler_fence;
#endif
}

static inline void
load_store_fence (uintptr_t dep)
{
#if !NO_FENCES && defined(__arm__)
  /* Fences the load that produced DEP. */
  __asm__ __volatile__ ("teq %0, %0; beq 0f; 0:" :: "r" (dep) : "memory");
#else
  (void) dep;
  __compiler_fence;
#endif
}

static inline void
store_load_fence ()
{
#if !NO_FENCES && defined(__arm__)
  __asm__ __volatile__ ("dmb" ::: "memory");
#elif !NO_FENCES
  __sync_synchronize ();
#else
  __compiler_fence;
#endif
}

static inline void
store_store_fence ()
{
#if !NO_FENCES && defined(__arm__)
  __asm__ __volatile__ ("dmb" ::: "memory");
#else
  __compiler_fence;
#endif
}

static inline bool
compare_and_swap (volatile size_t *ptr, size_t oldval, size_t newval)
{
#if !NO_FENCES || !NO_SYNC
#if !NO_LIGHTWEIGHT_CAS && defined(__arm__)
  int status = 1;
  __asm__ __volatile__ ("0: ldrex r0, [%1]\n\t"
                        "teq r0, %2\n\t"
			"bne 1f\n\t"
                        "strex %0, %3, [%1]\n\t"
                        "teq %0, #1\n\t"
                        "beq 0b\n\t"
                        "1:"
                        : "+r" (status)
                        : "r" (ptr), "r" (oldval), "r" (newval)
                        : "r0");
  return status == 0;
#else
  return __sync_bool_compare_and_swap (ptr, oldval, newval);
#endif
#else
  if (*ptr == oldval)
    {
      *ptr = newval;
      return true;
    }
  else
    return false;
#endif
}

static inline bool
weak_compare_and_swap (volatile size_t *ptr, size_t oldval, size_t newval)
{
#if !NO_FENCES || !NO_SYNC
#if !NO_LIGHTWEIGHT_CAS && defined(__arm__)
  int status = 1;
  __asm__ __volatile__ ("ldrex r0, [%1]\n\t"
			"teq r0, %2\n\t"
			"strexeq %0, %3, [%1]"
			: "+r" (status)
			: "r" (ptr), "r" (oldval), "r" (newval)
			: "r0");
  return !status;
#else
  return __sync_bool_compare_and_swap (ptr, oldval, newval);
#endif
#else
  return compare_and_swap (ptr, oldval, newval);
#endif
}

#if defined(__i386)
static inline  int64_t rdtsc() {
  int64_t x;
  __asm__ volatile ("rdtsc" : "=A" (x));
  return x;
}
#elif defined(__amd64)
static inline int64_t rdtsc() {
  int64_t a, d;
  __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
  return (d<<32) | a;
}
#elif defined(__aarch64__)
static inline int64_t rdtsc() {
  int64_t virtual_timer_value;
  __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(virtual_timer_value));
  return virtual_timer_value;
}
#else
# error "RDTSC is not defined for your architecture"
#endif

#if defined(__arm__)
static inline size_t
load_linked (volatile size_t *ptr)
{
  size_t value;
  __asm__ __volatile__ ("ldrex %0, [%1]" : "=r" (value) : "r" (ptr));
  return value;
}

static inline bool
store_conditional (volatile size_t *ptr, size_t value)
{
  size_t status = 1;
  __asm__ __volatile__ ("strex %0, %2, [%1]"
			: "+r" (status)
			: "r" (ptr), "r" (value));
  return status == 0;
}
#endif

#endif
