#ifndef SYNC_H
#define SYNC_H

#include <stddef.h>

/**
 * Contains routines that allow a benchmark to synchronize
 * with OpenStreams profiler.
 */

void openstream_start_hardware_counters(void);
void openstream_pause_hardware_counters(void);
int wstream_df_interleave_data(void* p, size_t size);

#endif
