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

  // TODO: Add accelerator
  std::string accelerators_names[] = {"Partial_trsm", "Partial_gemm", "Partial_syrk"};

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

// TODO: Add new functions here

paramlist init_params_trsm(long A, long B, int block_size)
{
  paramlist params({
    {"N", block_size},
    {"M", block_size},
    {"A_int_1", A},
    {"A_int_2", 0},
    {"lda", block_size},
    {"B_int_1", B},
    {"B_int_2", 0},
    {"ldb", block_size}
  });

  return params;
}

paramlist init_params_gemm(long A, long B, long C, int block_size)
{
  paramlist params({
    {"N", block_size},
    {"M", block_size},
    {"K", block_size},
    {"A_int_1", A},
    {"A_int_2", 0},
    {"lda", block_size},
    {"B_int_1", B},
    {"B_int_2", 0},
    {"ldb", block_size},
    {"C_int_1", C},
    {"C_int_2", 0},
    {"ldc", block_size}
  });

  return params;
}

paramlist init_params_syrk(long A, long C, int block_size)
{
  paramlist params({
    {"N", block_size},
    {"K", block_size},
    {"A_int_1", A},
    {"A_int_2", 0},
    {"lda", block_size},
    {"C_int_1", C},
    {"C_int_2", 0},
    {"ldc", block_size}
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
  std::map<std::string, int> params_lookup({
    {"Partial_trsm", 0},
    {"Partial_gemm", 1},
    {"Partial_syrk", 2}
  });

  paramlist params;

  // TODO: Add new accelerators here
  switch(params_lookup.at(fp->cl_data->accel_name))
  {
    case 0:
      // FIXME: Hack to match 3 streams task to standard CBLAS function
      memcpy(out_buffers[0].ptr, in_buffers[1].ptr, out_buffers[0].size);
      params = init_params_trsm(in_buffers[0].addr,
             out_buffers[0].addr, 64);
      break;
    case 1:
      // FIXME: Hack to match 4 streams task to standard CBLAS function
      memcpy(out_buffers[0].ptr, in_buffers[2].ptr, out_buffers[0].size);
      params = init_params_gemm(in_buffers[0].addr, in_buffers[1].addr,
             out_buffers[0].addr, 64);
      break;
    case 2:
      // FIXME: Hack to match 3 streams task to standard CBLAS function
      memcpy(out_buffers[0].ptr, in_buffers[1].ptr, out_buffers[0].size);
      params = init_params_syrk(in_buffers[0].addr,
             out_buffers[0].addr, 64);
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

