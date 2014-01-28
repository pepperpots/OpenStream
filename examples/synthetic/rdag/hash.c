/**
 * Copyright (C) 2014 Andi Drebes <andi.drebes@lip6.fr>
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

#include "hash.h"
#include <string.h>

int hash_table_hash_str(const void* pstr)
{
	int val = 0;
	const char* str = pstr;

	while(*str) {
		val ^= *str;
		str++;
	}

	return val;
}

int hash_table_cmp_str(const void* pa, const void* pb)
{
	return strcmp((const char*)pa, (const char*)pb) == 0;
}
