#ifndef _WSTREAM_DF_H_
#define _WSTREAM_DF_H_

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
load_load_fence ()
{
  __compiler_fence;
}

static inline void
load_store_fence ()
{
  __compiler_fence;
}

static inline void
store_load_fence ()
{
  __sync_synchronize ();
}

static inline void
store_store_fence ()
{
  __compiler_fence;
}



void dump_papi_counters (int);
void init_papi_counters (int);
void start_papi_counters (int);
void stop_papi_counters (int);
void accum_papi_counters (int);
#endif
