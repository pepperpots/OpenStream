#include "fos-manager.h"
#include "fpga-support.h"

wstream_fpga_env_p create_fpga_environment()
{
  wstream_fpga_env_p fpga_env_p = new wstream_fpga_env();

  UdmaRepo repo;

  fpga_env_p->device = repo.device(0);
  fpga_env_p->prmanager.fpgaLoadShell("Ultra96_100MHz_2");

  return fpga_env_p;
}

void execute_task_on_accelerator(wstream_df_frame_p fp, wstream_fpga_env_p fpga_env_p)
{
  wstream_df_view_p out_view_list[get_num_args(fp)];
  wstream_df_view_p in_view_list[get_num_args(fp)];

  unsigned out_index = 0;
  unsigned in_index = 0;

  unsigned num_args = get_num_args(fp);

  char* accelerator_memory = (char*) fpga_env_p->device->map();
  unsigned accelerator_memory_offset = 0; 

  // Create views for accelerator buffers
  wstream_physical_buffer_view in_buffers[get_num_args(fp)];
  wstream_physical_buffer_view out_buffers[get_num_args(fp)];

  // Process output arguments
  for (unsigned i = 0; i < 1; i++)
  {
      // Create view for output
      out_view_list[out_index] = get_arg_view(fp, i);
      wstream_df_frame_p cons = out_view_list[out_index]->owner;
      __built_in_wstream_df_prepare_data(out_view_list[out_index]);

      // Assign next free chunk of memory to the buffer
      out_buffers[out_index].ptr = accelerator_memory + accelerator_memory_offset;
      out_buffers[out_index].addr = fpga_env_p->device->phys_addr + accelerator_memory_offset;
      out_buffers[out_index].size = fp->cl_data->args[i].size;

      // Move pointer to point to the next free chunk
      accelerator_memory_offset += out_buffers[out_index].size;

      // Increment number of processed output buffers
      out_index++;
  }

  // Process input arguments
  for (unsigned i = 1; i < 3; i++)
  {
      // Create view for input
      in_view_list[in_index] = get_arg_view(fp, i);

      // Assign next free chunk of memory to the buffer
      in_buffers[in_index].ptr = accelerator_memory + accelerator_memory_offset;
      in_buffers[in_index].addr = fpga_env_p->device->phys_addr + accelerator_memory_offset;
      in_buffers[in_index].size = fp->cl_data->args[i].size;

      // Move pointer to point to the next free chunk
      accelerator_memory_offset += in_buffers[in_index].size;

      // Copy input data to accelerator memory
      memcpy(in_buffers[in_index].ptr, in_view_list[in_index]->data, in_buffers[in_index].size);

      // Increment number of processed input buffers
      in_index++;
  }

  // execute_hardcoded_vecadd
  paramlist params({
    {"C_1",   out_buffers[0].addr},
    {"C_2",   0},
    {"A_1",   in_buffers[0].addr},
    {"A_2",   0},
    {"B_1",   in_buffers[1].addr},
    {"B_2",   0},
    {"n",     256}
  });
  AccelInst accel = fpga_env_p->prmanager.fpgaRun(fp->cl_data->accel_name, params);
  accel.wait();

  // Release input views
  for (int i = 0; i < in_index; i++)
  {
    __built_in_wstream_df_dec_view_ref(in_view_list[i], 1);
  }

  // Mark output views as resolved
  for (int i = 0; i < out_index; i++)
  {
    // Copy data from accelerator
    memcpy(out_view_list[i]->data, out_buffers[i].ptr, out_buffers[i].size);

    __builtin_ia32_tdecrease_n(out_view_list[i]->owner, out_view_list[i]->burst, 1);
    __builtin_ia32_broadcast(out_view_list[i]);
  }

  __builtin_ia32_tend(fp);
}

void destroy_fpga_environment(wstream_fpga_env_p fpga_env_p)
{
  delete fpga_env_p;
}

