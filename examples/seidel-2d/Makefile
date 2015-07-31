include ../Makefile.config

TESTS = stream_df_seidel_2d stream_df_seidel_2d_reuse stream_seidel_2d_from_df stream_seidel_2d_seqctrl seidel_2d
ICC_TESTS = icc_stream_df_seidel_2d icc_stream_df_seidel_2d_reuse icc_stream_seidel_2d_from_df icc_stream_seidel_2d_seqctrl icc_seidel_2d
CFLAGS =  -Wall -g -std=c99 -O3 -Wunused-parameter -ffast-math $(PROFILER_CFLAGS)
ICC_CFLAGS = -g -std=c99 -fast
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc

all: $(TESTS)

all-icc: $(ICC_TESTS)

seidel_2d: seidel_2d.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline

icc_seidel_2d: seidel_2d.c ../common/common.c
	$(ICC) $(ICC_CFLAGS) $^ -o $@


stream_seidel_2d_from_df: stream_seidel_2d_from_df.c stream_seidel_2d_from_df_outline.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

icc_stream_seidel_2d_from_df: stream_seidel_2d_from_df.c stream_seidel_2d_from_df_outline_icc.a ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df $(ICC_STATIC_LIBS)


stream_seidel_2d_seqctrl: stream_seidel_2d_seqctrl.c stream_seidel_2d_seqctrl_outline.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

icc_stream_seidel_2d_seqctrl: stream_seidel_2d_seqctrl.c stream_seidel_2d_seqctrl_outline_icc.a ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df


stream_df_seidel_2d: stream_df_seidel_2d.c stream_df_seidel_2d_outline.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

icc_stream_df_seidel_2d: stream_df_seidel_2d.c stream_df_seidel_2d_outline_icc.a ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df $(ICC_STATIC_LIBS)


stream_df_seidel_2d_reuse: stream_df_seidel_2d_reuse.c stream_df_seidel_2d_reuse_outline.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

icc_stream_df_seidel_2d_reuse: stream_df_seidel_2d_reuse.c stream_df_seidel_2d_reuse_outline_icc.a ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df $(ICC_STATIC_LIBS)


%_icc.a: %.c
	@[ ! -z "$(ICC)" ] || (echo "Could not find ICC" ; exit 1)
	@[ ! -z "$(XIAR)" ] || (echo "Could not find XIAR" ; exit 1)
	$(ICC) $(ICC_CFLAGS) -o icc_$*.o -c $<
	$(XIAR) cr $@ icc_$*.o

DF_GEN_DEPS = ../../extras/generators/seidel/seidel.py \
		../../extras/generators/seidel/stream_df_seidel.py \
		../../extras/generators/seidel/seidel_df_common.py \
		../../extras/generators/seidel/seidel_par_common.py \
		../../extras/generators/seidel/seidel_common.py

DF_REUSE_GEN_DEPS = ../../extras/generators/seidel/seidel.py \
		../../extras/generators/seidel/stream_df_seidel_reuse.py \
		../../extras/generators/seidel/seidel_df_common.py \
		../../extras/generators/seidel/seidel_par_common.py \
		../../extras/generators/seidel/seidel_common.py

SHM_FROM_DF_GEN_DEPS = ../../extras/generators/seidel/seidel.py \
		../../extras/generators/seidel/stream_seidel_from_df.py \
		../../extras/generators/seidel/seidel_shm_common.py \
		../../extras/generators/seidel/seidel_par_common.py \
		../../extras/generators/seidel/seidel_common.py

SHM_SEQCTRL_GEN_DEPS = ../../extras/generators/seidel/seidel.py \
		../../extras/generators/seidel/stream_seidel_seqctrl.py \
		../../extras/generators/seidel/seidel_shm_common.py \
		../../extras/generators/seidel/seidel_par_common.py \
		../../extras/generators/seidel/seidel_common.py

SEQ_GEN_DEPS = ../../extras/generators/seidel/seidel.py \
		../../extras/generators/seidel/seidel_seq.py \
		../../extras/generators/seidel/seidel_common.py

stream_df_seidel_2d.c: $(DF_GEN_DEPS)
	$< --openstream-only --dimensions=2 --version=stream_df > $@

stream_df_seidel_2d_outline.c: $(DF_GEN_DEPS)
	$< --body-funcs-only --dimensions=2 --version=stream_df > $@

stream_df_seidel_2d_reuse.c: $(DF_REUSE_GEN_DEPS)
	$< --openstream-only --dimensions=2 --version=stream_df_reuse > $@

stream_df_seidel_2d_reuse_outline.c: $(DF_REUSE_GEN_DEPS)
	$< --body-funcs-only --dimensions=2 --version=stream_df_reuse > $@

stream_seidel_2d_from_df.c: $(SHM_FROM_DF_GEN_DEPS)
	$< --openstream-only --dimensions=2 --version=stream_from_df > $@

stream_seidel_2d_from_df_outline.c: $(SHM_FROM_DF_GEN_DEPS)
	$< --body-funcs-only --dimensions=2 --version=stream_from_df > $@

stream_seidel_2d_seqctrl.c: $(SHM_SEQCTRL_GEN_DEPS)
	$< --openstream-only --dimensions=2 --version=stream_seqctrl > $@

stream_seidel_2d_seqctrl_outline.c: $(SHM_SEQCTRL_GEN_DEPS)
	$< --body-funcs-only --dimensions=2 --version=stream_seqctrl > $@

seidel_2d.c: $(SEQ_GEN_DEPS)
	$< --dimensions=2 --version=seq > $@

clean:
	rm -f $(ICC_TESTS) $(TESTS) *~ *.c.* *.s *.raw *.out *.txt *.o *.a *.c
