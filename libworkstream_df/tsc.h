#ifndef TSC_H
#define TSC_H

uint64_t get_current_nanoseconds(void);
uint64_t get_tsc_ticks_per_second(void);

struct tsc_reference_offset {
	int ref_cpu;
	uint64_t ref_ts;
	uint64_t ref_tsc;
	uint64_t tsc_ticks_per_second;

	pthread_spinlock_t lock;
};

void tsc_reference_offset_init(struct tsc_reference_offset* ref);
int64_t get_tsc_offset(struct tsc_reference_offset* ref, int cpu);

extern struct tsc_reference_offset global_tsc_ref;

#endif
