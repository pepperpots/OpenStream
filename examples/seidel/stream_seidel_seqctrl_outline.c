/**
 * Outlined functions from stream_seidel_seqctrl.c
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

void gauss_seidel_df_in_place(double* matrix, int blocks_x, int blocks_y, int block_size, int id_x, int id_y)
{
	/* Depending on the position of an element within the block,
	 * different data sources must be used. For example, elements
	 * on the left side depend on left, those on the right side
	 * depend on right and so on. The different cases are treated
	 * outside of the main loop in order to avoid branching within
	 * the loop body.
	 *
	 * The loop bodies are very similar for all cases:
	 * First, the input elements top, left, right,
	 * bottom and center are determined in order to
	 * calculate the new value of the element. Afterwards,
	 * the matrix is updated.
	 */
	int N = blocks_x * block_size;

	for(int y = 0; y < block_size; y++) {
		for(int x = 0; x < block_size; x++) {
			int global_y = id_y * block_size + y;
			int global_x = id_x * block_size + x;

			double top_val = (y == 0 && id_y == 0) ? 0.0 : matrix[(global_y-1)*N + global_x];
			double left_val = (x == 0 && id_x == 0) ? 0.0 : matrix[global_y*N + global_x-1];
			double right_val = (x == block_size-1 && id_x == blocks_x-1) ? 0.0 : matrix[global_y*N + global_x+1];
			double bottom_val = (y == block_size-1 && id_y == blocks_y - 1) ? 0.0 : matrix[(global_y+1)*N + global_x];
			double center_val = matrix[global_y*N + global_x];
			double new_val = (top_val + left_val + right_val + bottom_val + center_val) * 0.2;

			matrix[global_y*N + global_x] = new_val;
		}
	}
}

#define UPD_IN_PLACE_BORDER(_x, _y) do { \
		int global_y = id_y * block_size + (_y);			\
		int global_x = id_x * block_size + (_x);			\
									\
		double top_val = ((_y) == 0 && id_y == 0) ? 0.0 : matrix[(global_y-1)*N + global_x]; \
		double left_val = ((_x) == 0 && id_x == 0) ? 0.0 : matrix[global_y*N + global_x-1]; \
		double right_val = ((_x) == block_size-1 && id_x == blocks_x-1) ? 0.0 : matrix[global_y*N + global_x+1]; \
		double bottom_val = ((_y) == block_size-1 && id_y == blocks_y - 1) ? 0.0 : matrix[(global_y+1)*N + global_x]; \
		double center_val = matrix[global_y*N + global_x];	\
		double new_val = (top_val + left_val + right_val + bottom_val + center_val) * 0.2; \
									\
		matrix[global_y*N + global_x] = new_val;		\
	} while(0)

#define UPD_IN_PLACE(_x, _y) matrix[(id_y * block_size + (_y))*N + (id_x * block_size + (_x))] = \
		(matrix[((id_y * block_size + (_y))-1)*N + (id_x * block_size + (_x))] + \
		 matrix[(id_y * block_size + (_y))*N + (id_x * block_size + (_x))-1] + \
		 matrix[(id_y * block_size + (_y))*N + (id_x * block_size + (_x))+1] + \
		 matrix[((id_y * block_size + (_y))+1)*N + (id_x * block_size + (_x))] + \
		 matrix[(id_y * block_size + (_y))*N + (id_x * block_size + (_x))]) * 0.2

#define MIN_IN_PLACE(x, y) (((x) < (y)) ? (x) : (y))

void gauss_seidel_df_in_place_unrolled(double* matrix, int blocks_x, int blocks_y, int block_size, int id_x, int id_y)
{
	int N = blocks_x * block_size;

	/* Left column */
	for(int y = 0; y < block_size; y++)
		UPD_IN_PLACE_BORDER(0, y);

	/* top row */
	for(int x = 1; x < block_size; x++)
		UPD_IN_PLACE_BORDER(x, 0);

	/* Center block */
	for(int y = 1; y < block_size-1; y++) {
		int unroll_factor = 16;
		int prolog_lim = MIN_IN_PLACE(block_size-1, unroll_factor);
		int epilog_start = block_size-1-((block_size-1) % unroll_factor);

		for(int x = 1; x < prolog_lim; x++)
			UPD_IN_PLACE(x, y);

		for(int x = prolog_lim; x < epilog_start; x += unroll_factor) {
			UPD_IN_PLACE(x+0, y);
			UPD_IN_PLACE(x+1, y);
			UPD_IN_PLACE(x+2, y);
			UPD_IN_PLACE(x+3, y);
			UPD_IN_PLACE(x+4, y);
			UPD_IN_PLACE(x+5, y);
			UPD_IN_PLACE(x+6, y);
			UPD_IN_PLACE(x+7, y);
			UPD_IN_PLACE(x+8, y);
			UPD_IN_PLACE(x+9, y);
			UPD_IN_PLACE(x+10, y);
			UPD_IN_PLACE(x+11, y);
			UPD_IN_PLACE(x+12, y);
			UPD_IN_PLACE(x+13, y);
			UPD_IN_PLACE(x+14, y);
			UPD_IN_PLACE(x+15, y);
		}

		for(int x = epilog_start; x < block_size-1; x++)
			UPD_IN_PLACE(x, y);
	}

	/* Right column */
	for(int y = 1; y < block_size-1; y++)
		UPD_IN_PLACE_BORDER(block_size-1, y);

	/* Bottom row */
	for(int x = 1; x < block_size; x++)
		UPD_IN_PLACE_BORDER(x, block_size-1);
}
