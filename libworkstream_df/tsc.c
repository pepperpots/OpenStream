#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include "arch.h"
#include "tsc.h"

struct tsc_reference_offset global_tsc_ref;

uint64_t get_current_nanoseconds(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	return 1000000000 * (uint64_t)ts.tv_sec + ts.tv_nsec;;
}

uint64_t get_tsc_ticks_per_second(void)
{
	uint64_t ts_start;
	uint64_t ts_end;
	uint64_t ts;

	uint64_t tsc_start;
	uint64_t tsc_end;

	ts_start = get_current_nanoseconds();
	tsc_start = rdtsc();

	do {
		ts = get_current_nanoseconds();
	} while(ts - ts_start < 10000000);

	ts_end = get_current_nanoseconds();
	tsc_end = rdtsc();

	return (tsc_end - tsc_start)*1000000000 / (ts_end - ts_start);
}

void tsc_reference_offset_init(struct tsc_reference_offset* ref)
{
	ref->ref_cpu = -1;
	ref->ref_ts = 0;
	ref->ref_tsc = 0;
	ref->tsc_ticks_per_second = get_tsc_ticks_per_second();

	pthread_spin_init(&ref->lock, PTHREAD_PROCESS_PRIVATE);
}

int64_t get_tsc_offset(struct tsc_reference_offset* ref, int cpu)
{
	uint64_t ts = get_current_nanoseconds();
	uint64_t tsc = rdtsc();

	if(ref->ref_cpu == -1) {
		pthread_spin_lock(&ref->lock);

		if(ref->ref_cpu == -1) {
			ref->ref_cpu = cpu;
			ref->ref_ts = ts;
			ref->ref_tsc = tsc;
		}

		pthread_spin_unlock(&ref->lock);
	}

	int64_t ts_diff = (int64_t)ts - (int64_t)ref->ref_ts;

	int64_t offset = -((ts_diff * ref->tsc_ticks_per_second) / 1000000000 - tsc + ref->ref_tsc);

//	printf("TSC offset of cpu %d is %"PRId64"\n", cpu, offset);

	return offset;
}
