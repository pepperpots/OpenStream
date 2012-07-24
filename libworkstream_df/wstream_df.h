#ifndef _WSTREAM_DF_H_
#define _WSTREAM_DF_H_

#include <stdbool.h>
#include <stdint.h>

#define WSTREAM_DF_DEQUE_LOG_SIZE 8
#define MAX_NUM_CORES 1024

#define __compiler_fence __asm__ __volatile__ ("" ::: "memory")

/* Get the frame pointer of the current thread */
extern void *__builtin_ia32_get_cfp ();

/* Create a new thread, with frame pointer size, and sync counter */
extern void *__builtin_ia32_tcreate (size_t, size_t, void *);


/* Decrease the synchronization counter by one */
extern void __builtin_ia32_tdecrease (void *);
/* Decrease the synchronization counter by one */
extern void __builtin_ia32_tdecrease_n (void *, size_t);


/* Destroy (free) the current thread */
extern void __builtin_ia32_tend ();


/* Allocate and return an array of streams.  */
extern void wstream_df_stream_ctor (void **, size_t);
extern void wstream_df_stream_array_ctor (void **, size_t, size_t);
/* Deallocate an array of streams.  */
extern void wstream_df_stream_dtor (void **, size_t);
/* Add a reference to a stream when passing it as firstprivate to a
   task.  */
extern void wstream_df_stream_reference (void *, size_t);


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
}

static inline bool
weak_compare_and_swap (volatile size_t *ptr, size_t oldval, size_t newval)
{
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
}


void dump_papi_counters (int);
void init_papi_counters (int);
void start_papi_counters (int);
void stop_papi_counters (int);
void accum_papi_counters (int);
#endif
