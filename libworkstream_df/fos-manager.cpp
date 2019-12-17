#include "cynq/fpga.h"

#include "fos-manager.h"
#include "fpga-support.h"

wstream_fpga_env::wstream_fpga_env(std::string shell_name)
{
  this->shell = Shell::loadFromJSON("../bitstreams/" + shell_name + ".json");

  int i = 0;
  for(auto& blank : this->shell.blanks)
  {
    this->regions[i++] = new Region(blank.first, blank.second, 
        this->shell.blockers[blank.first], this->shell.addrs[blank.first]);
  }

  std::string accelerators_names[] = {"Partial_vec_add"};

  for(auto& name : accelerators_names)
  {
    this->accelerators.emplace(name, Accel::loadFromJSON("../bitstreams/" + name + ".json"));
  }

  FPGAManager fpga0(0);
  fpga0.loadFull(this->shell.bitstream);
}

wstream_fpga_env_p create_fpga_environment()
{
  wstream_fpga_env_p fpga_env_p = new wstream_fpga_env("Ultra96_100MHz_2");

  UdmaRepo repo;

  for(int i = 0; i < wstream_fpga_env::NUMBER_SLOTS; i++)
    fpga_env_p->devices[i] = repo.device(i);

  return fpga_env_p;
}

paramlist init_params_vecadd(long C, long A, long B, int n)
{
  paramlist params({
    {"C_1",   C},
    {"C_2",   0},
    {"A_1",   A},
    {"A_2",   0},
    {"B_1",   B},
    {"B_2",   0},
    {"n",     n}
  });

  return params;
}

void execute_task_on_accelerator(wstream_df_frame_p fp, wstream_fpga_env_p fpga_env_p, int slot_id)
{
  wstream_df_view_p out_view_list[get_num_args(fp)];
  wstream_df_view_p in_view_list[get_num_args(fp)];

  int out_index = 0;
  int in_index = 0;

  int num_args = get_num_args(fp);

  // Mapping is cached so it is safe to call it here
  char* accelerator_memory = (char*) fpga_env_p->devices[slot_id]->map();
  int accelerator_memory_offset = 0; 

  // Create views for accelerator buffers
  wstream_physical_buffer_view in_buffers[get_num_args(fp)];
  wstream_physical_buffer_view out_buffers[get_num_args(fp)];

  for(int i = 0; i < num_args; i++)
  {
    if(get_arg_direction(fp, i) == CL_ARG_OUT)
    {
      // Create view for output
      out_view_list[out_index] = get_arg_view(fp, i);
      __built_in_wstream_df_prepare_data(out_view_list[out_index]);

      // Assign next free chunk of memory to the buffer
      out_buffers[out_index].ptr = accelerator_memory + accelerator_memory_offset;
      out_buffers[out_index].addr = fpga_env_p->devices[slot_id]->phys_addr + accelerator_memory_offset;
      out_buffers[out_index].size = fp->cl_data->args[i].size;

      // Move pointer to point to the next free chunk
      accelerator_memory_offset += out_buffers[out_index].size;

      // Increment number of processed output buffers
      out_index++;
    }
    else if (get_arg_direction(fp, i) == CL_ARG_IN)
    {
      // Create view for input
      in_view_list[in_index] = get_arg_view(fp, i);

      // Assign next free chunk of memory to the buffer
      in_buffers[in_index].ptr = accelerator_memory + accelerator_memory_offset;
      in_buffers[in_index].addr = fpga_env_p->devices[slot_id]->phys_addr + accelerator_memory_offset;
      in_buffers[in_index].size = fp->cl_data->args[i].size;

      // Move pointer to point to the next free chunk
      accelerator_memory_offset += in_buffers[in_index].size;

      // Copy input data to accelerator memory
      memcpy(in_buffers[in_index].ptr, in_view_list[in_index]->data, in_buffers[in_index].size);

      // Increment number of processed input buffers
      in_index++;
    }
  }

  // Create parameters list for a given accelerator
  // TODO: String is available on compile time is there
  // any way to reduce overhead of making the decision?
  std::map<std::string, int> params_lookup({
    {"Partial_vec_add", 0}
  });

  paramlist params;

  switch(params_lookup.at(fp->cl_data->accel_name))
  {
    case 0:
      params = init_params_vecadd(out_buffers[0].addr,
             in_buffers[0].addr, in_buffers[1].addr,
           out_buffers[0].size / sizeof(int));
      break;
  }

  // Run the accelerator
  AccelInst inst;

  Accel &accel = fpga_env_p->accelerators[fp->cl_data->accel_name];
  inst.accel = &accel;

  Bitstream &bitstream = accel.bitstreams.at(slot_id);
  inst.bitstream = &bitstream;

  inst.region = fpga_env_p->regions[slot_id];

  fpga_env_p->regions[slot_id]->loadAccel(accel, bitstream);

  inst.programAccel(params);
  inst.runAccel();
  inst.wait();

  fpga_env_p->regions[slot_id]->unloadAccel();

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
  for(int i = 0; i < wstream_fpga_env::NUMBER_SLOTS; i++)
    delete fpga_env_p->regions[i];

  delete fpga_env_p;
}

