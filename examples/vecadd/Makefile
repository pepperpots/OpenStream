include ../Makefile.config

TESTS = vecadd_df_fpga
CFLAGS =  -Wall -g -std=c99 -O3 -fno-tree-vectorize -fno-inline -ffast-math $(PROFILER_CFLAGS) -fno-openstream-trace-view-access #-fdump-tree-all-all #-Werror
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

vecadd_df_fpga: vecadd_df_fpga.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

clean:
	rm -f $(TESTS) *~ *.c.* *.o *.s *.raw *.out *.txt *.ost *.osb $(MEM_CST_TESTS_X86) $(MEM_CST_TESTS_ARM) $(TESTS_ICC)
