include ../Makefile.config

TESTS = stream_df_seidel_3d stream_df_seidel_3d_reuse stream_seidel_3d_from_df stream_seidel_3d_seqctrl seidel_3d
CFLAGS =  -Wall -g -std=c99 -O3 -Wunused-parameter -ffast-math $(PROFILER_CFLAGS)
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc

all: $(TESTS)

seidel_3d: seidel_3d.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_seidel_3d_from_df: stream_seidel_3d_from_df.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_seidel_3d_seqctrl: stream_seidel_3d_seqctrl.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_df_seidel_3d: stream_df_seidel_3d.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_df_seidel_3d_reuse: stream_df_seidel_3d_reuse.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_df_seidel_3d.c: ../../extras/generators/seidel/seidel.py ../../extras/generators/seidel/stream_df_seidel.py
	$< --dimensions=3 --version=stream_df > $@.tmp
	mv $@.tmp $@

stream_df_seidel_3d_reuse.c: ../../extras/generators/seidel/seidel.py  ../../extras/generators/seidel/stream_df_seidel_reuse.py
	$< --dimensions=3 --version=stream_df_reuse > $@.tmp
	mv $@.tmp $@

stream_seidel_3d_from_df.c: ../../extras/generators/seidel/seidel.py  ../../extras/generators/seidel/stream_seidel_from_df.py
	$< --dimensions=3 --version=stream_from_df > $@.tmp
	mv $@.tmp $@

stream_seidel_3d_seqctrl.c: ../../extras/generators/seidel/seidel.py  ../../extras/generators/seidel/stream_seidel_seqctrl.py
	$< --dimensions=3 --version=stream_seqctrl > $@.tmp
	mv $@.tmp $@

seidel_3d.c: ../../extras/generators/seidel/seidel.py ../../extras/generators/seidel/seidel_seq.py
	$< --dimensions=3 --version=seq > $@.tmp
	mv $@.tmp $@

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
