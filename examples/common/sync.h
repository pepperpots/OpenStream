#ifndef SYNC_H
#define SYNC_H

#define _POSIX_C_SOURCE 200112L
#include <stddef.h>
#include <stdlib.h>
#include <malloc.h>
#include <errno.h>

/**
 * Contains routines that allow a benchmark to synchronize
 * with OpenStreams profiler.
 */

void openstream_start_hardware_counters(void);
void openstream_pause_hardware_counters(void);
int wstream_df_interleave_data(void* p, size_t size);

static inline void* malloc_interleaved(size_t size)
{
	void* p = malloc(size);

	if(p) {
		if(wstream_df_interleave_data(p, size)) {
			free(p);
			return NULL;
		}
	}

	return p;
}

static inline int posix_memalign_interleaved(void **memptr, size_t alignment, size_t size)
{
	int err;

	if(!(err = posix_memalign(memptr, alignment, size))) {
		if(wstream_df_interleave_data(*memptr, size)) {
			free(*memptr);
			return ENOMEM;
		}
	}

	return err;
}

#endif
