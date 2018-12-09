#ifndef PRNG_H
#define PRNG_H

#include <limits.h>

static inline void prng_init(unsigned int* rands, unsigned int worker_id)
{
	*rands = 77777 + worker_id * 19;
}

static inline unsigned int prng_nextb(unsigned int* rands, unsigned int mod_bits)
{
	*rands = (*rands) * 1103515245 + 12345;
	unsigned int this_rands = (*rands);
	unsigned int val = 0;
	unsigned int mask = mod_bits < sizeof(mask)*8 ? (1 << mod_bits) - 1 : UINT_MAX;
	unsigned int total_bits = sizeof(val)*8;

	for(unsigned int i = 0; i < total_bits; i += mod_bits) {
		val = val ^ this_rands;
		this_rands = this_rands >> mod_bits;
	}

	return val & mask;
}

static inline unsigned int prng_nextn(unsigned int* rands, unsigned int mod)
{
	/* FIXME: Do not use static shift amount of 16 */
	return (prng_nextb(rands, sizeof(*rands)*8) >> 16) % mod;
}

#endif
