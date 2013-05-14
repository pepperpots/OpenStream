#ifndef WORK_DISTRIBUTION_H
#define WORK_DISTRIBUTION_H

#include "wstream_df.h"
#include "config.h"

#if !ALLOW_PUSH_REORDER
void reorder_pushes(wstream_df_thread_p cthread);
#endif

#if ALLOW_PUSHES
void import_pushes(wstream_df_thread_p cthread);
int work_push_beneficial(wstream_df_frame_p fp, wstream_df_thread_p cthread, int num_workers, int* target_worker);
int work_try_push(wstream_df_frame_p fp, int target_worker, wstream_df_thread_p cthread, wstream_df_thread_p wstream_df_worker_threads);
#endif

wstream_df_frame_p obtain_work(wstream_df_thread_p cthread, wstream_df_thread_p wstream_df_worker_threads, uint64_t* misses, uint64_t* allocator_misses);
#endif
