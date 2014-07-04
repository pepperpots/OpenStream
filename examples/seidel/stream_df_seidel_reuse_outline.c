/**
 * Outlined functions from stream_df_seidel_reuse.c
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

/* Performs an iteration on a single block.
 *
 * blocks_x, blocks_y:
 *   number of blocks in each direction of the matrix
 *
 * block_size:
 *   block width / height
 *
 * block_size:
 *   block width / height
 *
 * numiters:
 *   total number of iterations
 *
 * it:
 *   iteration of the task to be created
 * id_x, id_y: horizontal / vertical position of the block within the matrix
 *
 * left, top:
 *   data produced during the same iteration by the left / upper neighbor
 *
 * right, bottom, center:
 *   data produced during the previous iteration by the right, lower and
 *   upper neighbor and the block itself
 *
 * NULL values are passed for pointers that are not required
 * because of the block's location (e.g. left is NULL for
 * blocks on the left of the matrix; top is NULL for blocks
 * at the top of the matrix).
 */
void gauss_seidel_df_in_place(int block_size,
		     double* left, double* top,
		     double* bottom, double* right, double* center)
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

	for(int y = 0; y < block_size; y++) {
		for(int x = 0; x < block_size; x++) {
			double top_val = (y == 0)
				? (top ? top[x] : 0.0)
				: center[(y-1)*block_size + x];
			double left_val = (x == 0)
				? (left ? left[y] : 0.0)
				: center[y*block_size + (x-1)];
			double right_val = (x == block_size-1)
				? (right ? right[y] : 0.0)
				: center[y*block_size + (x + 1)];
			double bottom_val = (y == block_size-1)
				? (bottom ? bottom[x] : 0.0)
				: center[(y+1)*block_size + x];
			double center_val = center[y*block_size+x];
			double new_val = (top_val + left_val + right_val +
					  bottom_val + center_val) * 0.2;

			if(x == block_size-1 && right)
				right[y] = new_val;
			if(x == 0 && left)
				left[y] = new_val;
			if(y == block_size-1 && bottom)
				bottom[x] = new_val;
			if(y == 0 && top)
				top[x] = new_val;

			center[y*block_size+x] = new_val;
		}
	}
}

/* Copies data of a block of the initial matrix into
 * the vectors and the matrix of the output stream.
 *
 * matrix: Initial matrix in row-major order
 * id_x, id_y: Block coordinates
 * N: width / height of the matrix
 * block_size: width / height of a block
 * left_out, top_out, bottom_out, right_out, center_out:
 *   output data of this block for the next iteration
 */
void gauss_seidel_df_init(double* matrix, int id_x, int id_y, int N,
			  int block_size, double* left_out, double* top_out,
			  double* bottom_out, double* right_out,
			  double* center_out)
{
	for(int y = 0; y < block_size; y++) {
		for(int x = 0; x < block_size; x++) {
			int global_x = id_x*block_size+x;
			int global_y = id_y*block_size+y;

			double curr = matrix[global_y*N+global_x];

			center_out[y*block_size+x] = curr;

			if(x == 0 && left_out)
				left_out[y] = curr;
			else if(x == block_size-1 && right_out)
				right_out[y] = curr;

			if(y == 0 && top_out)
				top_out[x] = curr;
			else if(y == block_size-1 && bottom_out)
				bottom_out[x] = curr;
		}
	}
}

/* Inverse operation of gauss_seidel_df_init:
 * Copies the final data from the stream buffers into a matrix.
 *
 * matrix: Final matrix in row-major order
 * id_x, id_y: Block coordinates
 * N: width / height of the matrix
 * block_size: width / height of a block
 * left_in, top_in, bottom_in, right_in, center_in:
 *   output data of the blocks at the last iteration
 */
void gauss_seidel_df_finish(double* matrix, int id_x, int id_y, int N,
			    int block_size, double* left_in, double* top_in,
			    double* bottom_in, double* right_in,
			    double* center_in)
{
	int foo = 0;
	for(int y = 0; y < block_size; y++) {
		for(int x = 0; x < block_size; x++) {
			double curr = center_in[y*block_size+x];

			int global_x = id_x*block_size+x;
			int global_y = id_y*block_size+y;

			if(x == 0 && left_in)
				curr = left_in[y];
			else if(x == block_size-1 && right_in)
				curr = right_in[y];

			if(y == 0 && top_in)
				curr = top_in[x];
			else if(y == block_size-1 && bottom_in)
				curr = bottom_in[x];

			matrix[global_y*N+global_x] = curr;
		}
	}
}
