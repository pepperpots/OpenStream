#ifndef _FPGA_SUPPORT_H_
#define _FPGA_SUPPORT_H_

extern "C" {

#include "wstream_df.h"


/* FPGA support data structures. */

// Similar to OpenCL data structures
enum cl_arg_direction
{
  CL_ARG_UNDEFINED    = 0,
  CL_ARG_IN           = 1,
  CL_ARG_OUT          = 2,
  CL_ARG_INOUT        = 3,
  CL_ARG_FIRSTPRIVATE = 4
};

struct wstream_fpga_arg;
typedef struct wstream_fpga_arg
{
  size_t size;
  void **data;
  enum cl_arg_direction dir;

} wstream_fpga_arg_t, *wstream_fpga_arg_p;

typedef struct wstream_cl_data
{
  const char *accel_name;
  int args_count;
  int dimensions;
  int accelerator_no;
  int queue_no;

  /* Convenience pointers in same struct (within the __data area).
     Also serve as flags initialized NULL.  */
  wstream_fpga_arg_p args;
  size_t *work_offsets;
  size_t *work_sizes;

  /* Count how many have been already set.  Should use sanity check on
     the count.  */
  int args_counter, gwo_counter, gws_counter;

  // Placeholder/baseline address for address of the following data
  char __data[];
} wstream_cl_data_t, *wstream_cl_data_p;

void __built_in_wstream_df_setup_fpga_accel (void *, const char *, int, int);
void __built_in_wstream_df_set_fpga_arg (void *, void *, size_t, int);
void __built_in_wstream_df_set_fpga_work_offset (void *, size_t);
void __built_in_wstream_df_set_fpga_work_size (void *, size_t);

#  include "fpga-support.c.h"

}
#endif