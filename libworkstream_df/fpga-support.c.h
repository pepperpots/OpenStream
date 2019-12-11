static inline bool fpga_available (wstream_df_frame_p fp)
{
  return fp->cl_data != NULL;
}

static inline const char *get_accel_name (wstream_df_frame_p fp)
{
  assert (fp->cl_data);
  return fp->cl_data->accel_name;
}

static inline size_t get_num_dimensions (wstream_df_frame_p fp)
{
  assert (fp->cl_data);
  return fp->cl_data->dimensions;
}

static inline size_t get_num_args (wstream_df_frame_p fp)
{
  /* Check that the number of args registered is correct.  */
  assert (fp->cl_data);
  assert (fp->cl_data->args_count == fp->cl_data->args_counter);
  return fp->cl_data->args_count;
}

static inline size_t get_arg_size (wstream_df_frame_p fp, int index)
{
  assert (fp->cl_data);
  assert (index < fp->cl_data->args_counter);
  return fp->cl_data->args[index].size;
}

static inline void *get_arg_data_ptr (wstream_df_frame_p fp, int index)
{
  void *ret;
  assert (fp->cl_data);
  assert (index < fp->cl_data->args_counter);
  ret = *fp->cl_data->args[index].data;
  return ret;
}

static inline enum cl_arg_direction get_arg_direction (wstream_df_frame_p fp, int index)
{
  assert (fp->cl_data);
  assert (index < fp->cl_data->args_counter);
  return fp->cl_data->args[index].dir;
}

static inline size_t *get_work_offsets (wstream_df_frame_p fp)
{
  assert (fp->cl_data);
  if (fp->cl_data->gwo_counter == 0)
    return NULL;
  if (fp->cl_data->gwo_counter != fp->cl_data->dimensions)
    wstream_df_fatal ("OpenStream - CL-support: The number of GLOBAL WORK OFFSETs provided is different from the number of dimensions specified.");
  return fp->cl_data->work_offsets;
}

static inline size_t *get_work_sizes (wstream_df_frame_p fp)
{
  assert (fp->cl_data);
  if (fp->cl_data->dimensions == 0)
    {
      if (fp->cl_data->gws_counter != 0)
	// For now consider an error ... could be a warning.
	wstream_df_fatal ("OpenStream - CL-support: GLOBAL WORK SIZEs has been specified, but the number of dimensions has not.");
      else
	return NULL;
    }
  if (fp->cl_data->gws_counter == 0 && fp->cl_data->dimensions != 0)
    wstream_df_fatal ("OpenStream - CL-support: The number of GLOBAL WORK SIZEs cannot be 0 if the number of dimensions is specified.");
  if (fp->cl_data->gws_counter != fp->cl_data->dimensions)
    wstream_df_fatal ("OpenStream - CL-support: The number of GLOBAL WORK SIZEs provided is different from the number of dimensions specified.");
  return fp->cl_data->work_sizes;
}

static inline wstream_df_view_p get_arg_view (wstream_df_frame_p fp, int index)
{
  assert (fp->cl_data);
  assert (index < fp->cl_data->args_counter);
  assert (fp->cl_data->args[index].dir != CL_ARG_FIRSTPRIVATE);
  return (wstream_df_view_p) ((char *) fp->cl_data->args[index].data - sizeof (void *));
}

static inline bool is_cpu_task(wstream_df_frame_p frame)
{
  if (!fpga_available(frame))
  {
    return true;
  }
  else if (frame->cl_data->accelerator_no < 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}

static inline bool is_fpga_task(wstream_df_frame_p frame)
{
  if (frame && fpga_available(frame) && frame->cl_data->accelerator_no >= 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}

static inline int get_accelerator_no(wstream_df_frame_p frame)
{
  return frame->cl_data->accelerator_no;
}