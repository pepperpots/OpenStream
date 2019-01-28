include ../Makefile.config

TESTS = seidel stream_seidel topsort_deps_stream_seidel starss_to_stream_seidel openmp_task_seidel openmp_loop_seidel stream_df_seidel stream_df_seidel_reuse stream_seidel_from_df stream_seidel_seqctrl stream_seidel_df
TESTS_ICC=icc_stream_df_seidel icc_stream_df_seidel_reuse icc_stream_seidel_from_df icc_stream_seidel_seqctrl
CFLAGS =  -Wall -g -std=c99 -O3  -ffast-math $(PROFILER_CFLAGS) #-fdump-tree-all #-Werror
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

ifneq ($(MCC),)
TESTS += starss_seidel
endif

all: $(TESTS)

compare_outputs: compare_outputs.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@

seidel: seidel.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

icc_seidel: seidel.c ../common/common.c
	@[ ! -z "$(ICC)" ] || (echo "Could not find ICC" ; exit 1)
	$(ICC) -std=c99 -O3 $^ -o $@

stream_seidel: stream_seidel.c ../common/common.c ../common/sync.c
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_seidel_df: stream_seidel_df.c ../common/common.c ../common/sync.c
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_seidel_from_df: stream_seidel_from_df.c ../common/common.c ../common/sync.c stream_seidel_from_df_outline.o
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_seidel_seqctrl: stream_seidel_seqctrl.c ../common/common.c ../common/sync.c stream_seidel_seqctrl_outline.o
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_df_seidel: stream_df_seidel.c ../common/common.c ../common/sync.c stream_df_seidel_outline.o
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_df_seidel_reuse: stream_df_seidel_reuse.c ../common/common.c ../common/sync.c stream_df_seidel_reuse_outline.o
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

icc_stream_seidel_from_df: stream_seidel_from_df.c ../common/common.c ../common/sync.c stream_seidel_from_df_outline_icc.o
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

icc_stream_seidel_seqctrl: stream_seidel_seqctrl.c ../common/common.c ../common/sync.c stream_seidel_seqctrl_outline_icc.o
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

icc_stream_df_seidel: stream_df_seidel.c ../common/common.c ../common/sync.c stream_df_seidel_outline_icc.o
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

icc_stream_df_seidel_reuse: stream_df_seidel_reuse.c ../common/common.c ../common/sync.c stream_df_seidel_reuse_outline_icc.o
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

topsort_deps_stream_seidel: topsort_deps_stream_seidel.c ../common/common.c ../common/sync.c
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

starss_to_stream_seidel: starss_to_stream_seidel.c ../common/common.c ../common/sync.c
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

starss_seidel: starss_seidel.c ../common/common.c
	$(MCC) --ompss $^ -o $@  -O3 -g

openmp_task_seidel: openmp_task_seidel.c ../common/common.c
	$(PLAIN_GCC) $^ -o $@ -fopenmp -O3 -ffast-math -std=c99

openmp_loop_seidel: openmp_loop_seidel.c ../common/common.c
	$(PLAIN_GCC) $^ -o $@ -fopenmp -O3 -ffast-math -std=c99

openmp_task_seidel_interleaved: openmp_task_seidel_interleaved.c ../common/common.c ../../libworkstream_df/interleave.c
	$(PLAIN_GCC) $^ -o $@ -fopenmp -O3 -ffast-math -std=c99 -I../../install/include/ -L../../install/lib64 -lnuma

openmp_loop_seidel_interleaved: openmp_loop_seidel_interleaved.c ../common/common.c ../../libworkstream_df/interleave.c
	$(PLAIN_GCC) $^ -o $@ -fopenmp -O3 -ffast-math -std=c99 -I../../install/include/ -L../../install/lib64 -lnuma

%_outline.o: %_outline.c
	$(GCC) $(CFLAGS) $(LDFLAGS) -c $^ -o $@

%_outline_icc.o: %_outline.c
	@[ ! -z "$(ICC)" ] || (echo "Could not find ICC" ; exit 1)
	$(ICC) -std=c99 -O3 -c $^ -o $@

clean:
	rm -f $(TESTS) *~ *.c.* *.o *.s *.raw *.out *.txt $(MEM_CST_TESTS_X86) $(MEM_CST_TESTS_ARM) $(TESTS_ICC)

cilk_seidel: cilk_seidel.cilk ../common/common.c
	$(CILKC) -O2 -Wall $^ -o $@ -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200809L
