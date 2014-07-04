/**
 * Outlined functions from stream_seidel_from_df.c
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
