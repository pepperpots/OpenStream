/**
 * Copyright (C) 2013 Andi Drebes <andi.drebes@lip6.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef ANSI_EXTRAS_H
#define ANSI_EXTRAS_H

#include <endian.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <malloc.h>

#define SWAP_BITS(val, ret, bits) \
	do { \
		char* cval = (char*)&val; \
		for(unsigned int i = 0; i < (bits) / 8; ++i) \
			ret |= cval[i] << (((bits)-8) - 8*i); \
	} while(0)

inline int64_t int64_swap(int64_t val)
{
	int64_t ret;
	SWAP_BITS(val, ret, sizeof(int64_t)*8);

	return ret;
}

inline int32_t int32_swap(int32_t val)
{
	int32_t ret;
	SWAP_BITS(val, ret, sizeof(int32_t)*8);

	return ret;
}

inline int16_t int16_swap(int16_t val)
{
	int16_t ret;
	SWAP_BITS(val, ret, sizeof(int16_t)*8);

	return ret;
}

#if __BYTE_ORDER == __LITTLE_ENDIAN
	#define int16_htole(val) val
	#define int32_htole(val) val
	#define int64_htole(val) val
	#define int16_letoh(val) val
	#define int32_letoh(val) val
	#define int64_letoh(val) val
#elif __BYTE_ORDER == __BIG_ENDIAN
	#define int16_htole(val) int16_swap(val)
	#define int32_htole(val) int32_swap(val)
	#define int64_htole(val) int64_swap(val)
	#define int16_letoh(val) int16_swap(val)
	#define int32_letoh(val) int32_swap(val)
	#define int64_letoh(val) int64_swap(val)
#else
	#error "Could not determine your system's endianness"
#endif

/* Very simple string replacement function.
 * haystack must contain enough space for all
 * replacements*/
static inline void strreplace(char* haystack, const char* needle, const char* replacement)
{
	char* pos;
	int needle_len = strlen(needle);
	int replacement_len = strlen(replacement);

	while((pos = strstr(haystack, needle))) {
		memmove(pos+(replacement_len-needle_len), pos, strlen(pos)+1);
		memcpy(pos, replacement, replacement_len);
	}
}

static inline off_t file_size(const char* filename)
{
	struct stat stat_buf;

	if(stat(filename, &stat_buf) == -1)
		return -1;

	return stat_buf.st_size;
}

#endif
