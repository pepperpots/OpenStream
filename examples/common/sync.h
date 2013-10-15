#ifndef SYNC_H
#define SYNC_H

/**
 * Contains routines that allow a benchmark to synchronize
 * with OpenStreams profiler.
 */

void openstream_start_hardware_counters(void);
void openstream_pause_hardware_counters(void);

#endif
