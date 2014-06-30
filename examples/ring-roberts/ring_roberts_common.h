/**
 * Helper functions for ring roberts benchmarks.
 *
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
#include <sys/types.h>
#include <stdio.h>

#define GLOBAL_VAL(x, y) matrix[(id_y*block_size_y+(y))*N_x + id_x*block_size_x+(x)]
#define GLOBAL_TMP_VAL(x, y) tmpmatrix[(id_y*block_size_y+(y))*N_x + id_x*block_size_x+(x)]

#define GLOBAL_AVG(x, y) ((GLOBAL_VAL((x)-1, (y)-1) +	     \
			   GLOBAL_VAL(  (x), (y)-1) +	     \
			   GLOBAL_VAL((x)+1, (y)-1) +	     \
			   GLOBAL_VAL((x)-1, (y)) +	     \
			   GLOBAL_VAL(  (x), (y)) +	     \
			   GLOBAL_VAL((x)+1, (y)) +	     \
			   GLOBAL_VAL((x)-1, (y)+1) +	     \
			   GLOBAL_VAL(  (x), (y)+1) +			\
			   GLOBAL_VAL((x)+1, (y)+1)) / 8.0)

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

static inline void dump_matrix_2d(double* matrix, FILE* fp, ssize_t N_y, ssize_t N_x)
{
	size_t pos;

	for(ssize_t y = 0; y < N_y; y++) {
		for(ssize_t x = 0; x < N_x; x++) {
			pos = 0 + y*N_x + x;

			fprintf(fp, "%f\t", matrix[pos]);
		}

		fprintf(fp, "\n");
	}
}

static inline void init_matrix(double* matrix, ssize_t N_x, ssize_t N_y)
{
	for(ssize_t y = 0; y < N_y; y++) {
		for(ssize_t x = 0; x < N_x; x++) {
			size_t pos = 0 + y*N_x + x;
			matrix[pos] = (double)(x+y);
		}
	}
}
