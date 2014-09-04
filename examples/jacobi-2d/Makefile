include ../Makefile.config

TESTS = stream_df_jacobi_2d stream_df_jacobi_2d_reuse stream_jacobi_2d_from_df stream_jacobi_2d_seqctrl jacobi_2d
CFLAGS =  -Wall -g -std=c99 -O3 -Wunused-parameter -ffast-math $(PROFILER_CFLAGS)
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc

all: $(TESTS)

jacobi_2d: jacobi_2d.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_jacobi_2d_from_df: stream_jacobi_2d_from_df.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_jacobi_2d_seqctrl: stream_jacobi_2d_seqctrl.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_df_jacobi_2d: stream_df_jacobi_2d.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_df_jacobi_2d_reuse: stream_df_jacobi_2d_reuse.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_df_jacobi_2d.c: ../../extras/generators/jacobi/jacobi.py ../../extras/generators/jacobi/stream_df_jacobi.py
	$< --dimensions=2 --version=stream_df > $@.tmp
	mv $@.tmp $@

stream_df_jacobi_2d_reuse.c: ../../extras/generators/jacobi/jacobi.py  ../../extras/generators/jacobi/stream_df_jacobi_reuse.py
	$< --dimensions=2 --version=stream_df_reuse > $@.tmp
	mv $@.tmp $@

stream_jacobi_2d_from_df.c: ../../extras/generators/jacobi/jacobi.py  ../../extras/generators/jacobi/stream_jacobi_from_df.py
	$< --dimensions=2 --version=stream_from_df > $@.tmp
	mv $@.tmp $@

stream_jacobi_2d_seqctrl.c: ../../extras/generators/jacobi/jacobi.py  ../../extras/generators/jacobi/stream_jacobi_seqctrl.py
	$< --dimensions=2 --version=stream_seqctrl > $@.tmp
	mv $@.tmp $@

jacobi_2d.c: ../../extras/generators/jacobi/jacobi.py ../../extras/generators/jacobi/jacobi_seq.py
	$< --dimensions=2 --version=seq > $@.tmp
	mv $@.tmp $@

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
