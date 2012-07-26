#ifndef ATOMIC_C11_DEFS
#define ATOMIC_C11_DEFS

#if !USE_SEQ_CST_STDATOMIC
#define relaxed memory_order_relaxed
#define acquire memory_order_acquire
#define release memory_order_release
#define consume memory_order_consume
#define rel_acq memory_order_rel_acq
#define seq_cst memory_order_seq_cst
#define thread_fence(memord) atomic_thread_fence (memord);
#else
#define relaxed memory_order_seq_cst
#define acquire memory_order_seq_cst
#define release memory_order_seq_cst
#define consume memory_order_seq_cst
#define rel_acq memory_order_seq_cst
#define seq_cst memory_order_seq_cst
#define thread_fence(memord)
#endif

#endif
