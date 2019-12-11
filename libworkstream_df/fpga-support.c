#include "alloc.h"
#include "fpga-support.h"

extern __thread wstream_df_thread_p current_thread;

void
__built_in_wstream_df_setup_fpga_accel (void *frame,
				       const char *accel_name,
				       int args_count,
				       int dimensions)
{
  wstream_df_thread_p cthread = current_thread;
  wstream_df_frame_p fp = (wstream_df_frame_p) frame;
  size_t size_args, size_dims, size;
  assert (fp->cl_data == NULL);

  size_args = args_count * sizeof(wstream_fpga_arg_t);
  size_dims = dimensions * sizeof(size_t);
  // size of structure itself + cl_args + cl_global_work_offsets + cl_global_work_sizes + cl_local_work_sizes
  size = sizeof(wstream_cl_data_t) + size_args + 3*size_dims;

  fp->cl_data = (wstream_cl_data_p) slab_alloc(cthread, cthread->slab_cache, size);

  fp->cl_data->accel_name = accel_name;
  fp->cl_data->args_count = args_count;
  fp->cl_data->dimensions = dimensions;
  fp->cl_data->accelerator_no = -1; // -1 means not an fpga task, 0 and above is for the device number
  fp->cl_data->queue_no = 0;

  fp->cl_data->args =        (wstream_fpga_arg_p)  &fp->cl_data->__data[0];
  fp->cl_data->work_offsets = (size_t *)  &fp->cl_data->__data[size_args];
  fp->cl_data->work_sizes =   (size_t *)  &fp->cl_data->__data[size_args + size_dims];

  fp->cl_data->args_counter = 0;
  fp->cl_data->gwo_counter  = 0;
  fp->cl_data->gws_counter  = 0;
}

void
__built_in_wstream_df_set_fpga_arg (void *frame,
				  void *data,
				  size_t size,
				  int direction)
{
  wstream_df_frame_p fp = (wstream_df_frame_p) frame;

  // Use a reverse counting as the clauses are traversed in reverse order
  int arg_index = fp->cl_data->args_count - ++fp->cl_data->args_counter;

  // Make sure we're not trying to add too many arguments
  assert (arg_index >= 0);
//  assert (direction > CL_ARG_UNDEFINED && direction <= CL_ARG_FIRSTPRIVATE);

  fp->cl_data->args[arg_index].size = size;
  fp->cl_data->args[arg_index].data = data;
  fp->cl_data->args[arg_index].dir = direction;
}

void
__built_in_wstream_df_set_fpga_work_offset (void *frame,
						 size_t offset)
{
  wstream_df_frame_p fp = (wstream_df_frame_p) frame;
  int index = fp->cl_data->dimensions - ++fp->cl_data->gwo_counter;
  assert (index >= 0);
  fp->cl_data->work_offsets[index] = offset;
}

void
__built_in_wstream_df_set_fpga_work_size (void *frame,
					       size_t size)
{
  wstream_df_frame_p fp = (wstream_df_frame_p) frame;
  int index = fp->cl_data->dimensions - ++fp->cl_data->gws_counter;
  assert (index >= 0);
  fp->cl_data->work_sizes[index] = size;
}
