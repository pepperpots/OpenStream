/**
 * Helper functions for Cholesky benchmarks.
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

#include <stdio.h>

static inline void matrix_add_padding(double* matrix, int N, int padding_elements)
{
	if(padding_elements == 0)
		return;

	/* Re-align lines taking into account per-line padding */
	for(int i = N-1; i >= 0; i--)
		memmove(&matrix[i*(N+padding_elements)], &matrix[i*N], N*sizeof(double));
}

static inline void matrix_strip_padding(double* matrix, int N, int padding_elements)
{
	if(padding_elements == 0)
		return;

	/* Remove padding from global matrix */
	for(int i = 0; i < N; i++)
		memmove(&matrix[i*N], &matrix[i*(N+padding_elements)], N*sizeof(double));
}

static inline void dump_matrix_2d(double* matrix, FILE* fp, int N_y, int N_x)
{
	size_t pos;

	for(int y = 0; y < N_y; y++) {
		for(int x = 0; x < N_x; x++) {
			pos = 0 + y*N_x + x;

			fprintf(fp, "%f\t", matrix[pos]);
		}

		fprintf(fp, "\n");
	}
}
