/**
 * Outlined functions from stream_df_seidel.c
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
 * left_in, top_in:
 *   data produced during the same iteration by the left / upper neighbor
 *
 * right_in, bottom_in, center_in:
 *   data produced during the previous iteration by the right, lower and
 *   upper neighbor and the block itself
 *
 * right_out, bottom_out:
 *   output data of this block for the right / lower neighbor of the same
 *   iteration.
 *
 * left_out, top_out, center_out:
 *   output data of this block for the next iteration
 *
 * NULL values are passed for pointers that are not required
 * because of the block's location (e.g. left_in is NULL for
 * blocks on the left of the matrix; top_out is NULL for blocks
 * at the top of the matrix).
 */
void gauss_seidel_df(int block_size, double* left_in, double* top_in,
		     double* bottom_in, double* right_in, double* center_in,
		     double* left_out, double* top_out, double* bottom_out,
		     double* right_out, double* center_out)
{
	/* Depending on the position of an element within the block,
	 * different data sources must be used. For example, elements
	 * on the left side depend on left_in, those on the right side
	 * depend on right_in and so on. The different cases are treated
	 * outside of the main loop in order to avoid branching within
	 * the loop body.
	 *
	 * The loop bodies are very similar for all cases:
	 * First, the input elements top_val, left_val, right_val,
	 * bottom_val and center_val are determined in order to
	 * calculate the new value of the element. Afterwards,
	 * the output matrix is updated.
	 *
	 * If the element is a part of the another block's input at
	 * the next iteration, the appropriate output vector is updated
	 * as well.
	 */

	for(int y = 0; y < block_size; y++) {
		for(int x = 0; x < block_size; x++) {
			double top_val = (y == 0)
				? (top_in ? top_in[x] : 0.0)
				: center_out[(y-1)*block_size + x];
			double left_val = (x == 0)
				? (left_in ? left_in[y] : 0.0)
				: center_out[y*block_size + (x-1)];
			double right_val = (x == block_size-1)
				? (right_in ? right_in[y] : 0.0)
				: center_in[y*block_size + (x + 1)];
			double bottom_val = (y == block_size-1)
				? (bottom_in ? bottom_in[x] : 0.0)
				: center_in[(y+1)*block_size + x];
			double center_val = center_in[y*block_size+x];
			double new_val = (top_val + left_val + right_val +
					  bottom_val + center_val) * 0.2;

			if(x == block_size-1 && right_out)
				right_out[y] = new_val;
			if(x == 0 && left_out)
				left_out[y] = new_val;
			if(y == block_size-1 && bottom_out)
				bottom_out[x] = new_val;
			if(y == 0 && top_out)
				top_out[x] = new_val;

			center_out[y*block_size+x] = new_val;
		}
	}
}

#define UPD_OUT_OF_PLACE_BORDER(_x, _y) do {				\
		double top_val = ((_y) == 0)				\
			? (top_in ? top_in[(_x)] : 0.0)		\
			: center_out[((_y)-1)*block_size + (_x)];	\
		double left_val = ((_x) == 0)				\
			? (left_in ? left_in[(_y)] : 0.0)		\
			: center_out[(_y)*block_size + ((_x)-1)];	\
		double right_val = ((_x) == block_size-1)		\
			? (right_in ? right_in[(_y)] : 0.0)		\
			: center_in[(_y)*block_size + ((_x) + 1)];	\
		double bottom_val = ((_y) == block_size-1)		\
			? (bottom_in ? bottom_in[(_x)] : 0.0)		\
			: center_in[((_y)+1)*block_size + (_x)];	\
		double center_val = center_in[(_y)*block_size+(_x)];	\
		double new_val = (top_val + left_val + right_val +	\
				  bottom_val + center_val) * 0.2;	\
									\
		if((_x) == block_size-1 && right_out)			\
			right_out[(_y)] = new_val;			\
		if((_x) == 0 && left_out)				\
			left_out[(_y)] = new_val;			\
		if((_y) == block_size-1 && bottom_out)			\
			bottom_out[(_x)] = new_val;			\
		if((_y) == 0 && top_out)				\
			top_out[(_x)] = new_val;			\
									\
		center_out[(_y)*block_size+(_x)] = new_val;		\
	} while(0)

#define UPD_OUT_OF_PLACE(_x, _y) center_out[(_y)*block_size+(_x)] =	\
		(center_out[((_y)-1)*block_size + (_x)] +		\
		 center_out[(_y)*block_size + ((_x)-1)] +		\
		 center_in[(_y)*block_size + ((_x) + 1)] +		\
		 center_in[((_y)+1)*block_size + (_x)] +		\
		 center_in[(_y)*block_size+(_x)]) * 0.2

#define MIN_OUT_OF_PLACE(x, y) (((x) < (y)) ? (x) : (y))

void gauss_seidel_df_unrolled(int block_size, double* left_in, double* top_in,
		     double* bottom_in, double* right_in, double* center_in,
		     double* left_out, double* top_out, double* bottom_out,
		     double* right_out, double* center_out)
{
	/* Left column */
	for(int y = 0; y < block_size; y++)
		UPD_OUT_OF_PLACE_BORDER(0, y);

	/* top row */
	for(int x = 1; x < block_size; x++)
		UPD_OUT_OF_PLACE_BORDER(x, 0);

	/* Center block */
	for(int y = 1; y < block_size-1; y++) {
		int unroll_factor = 16;
		int prolog_lim = MIN_OUT_OF_PLACE(block_size-1, unroll_factor);
		int epilog_start = block_size-1-((block_size-1) % unroll_factor);

		for(int x = 1; x < prolog_lim; x++)
			UPD_OUT_OF_PLACE(x, y);

		for(int x = prolog_lim; x < epilog_start; x += unroll_factor) {
			UPD_OUT_OF_PLACE(x+0, y);
			UPD_OUT_OF_PLACE(x+1, y);
			UPD_OUT_OF_PLACE(x+2, y);
			UPD_OUT_OF_PLACE(x+3, y);
			UPD_OUT_OF_PLACE(x+4, y);
			UPD_OUT_OF_PLACE(x+5, y);
			UPD_OUT_OF_PLACE(x+6, y);
			UPD_OUT_OF_PLACE(x+7, y);
			UPD_OUT_OF_PLACE(x+8, y);
			UPD_OUT_OF_PLACE(x+9, y);
			UPD_OUT_OF_PLACE(x+10, y);
			UPD_OUT_OF_PLACE(x+11, y);
			UPD_OUT_OF_PLACE(x+12, y);
			UPD_OUT_OF_PLACE(x+13, y);
			UPD_OUT_OF_PLACE(x+14, y);
			UPD_OUT_OF_PLACE(x+15, y);
		}

		for(int x = epilog_start; x < block_size-1; x++)
			UPD_OUT_OF_PLACE(x, y);
	}

	/* Right column */
	for(int y = 1; y < block_size-1; y++)
		UPD_OUT_OF_PLACE_BORDER(block_size-1, y);

	/* Bottom row */
	for(int x = 1; x < block_size; x++)
		UPD_OUT_OF_PLACE_BORDER(x, block_size-1);
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
