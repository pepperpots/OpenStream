include ../Makefile.config

TESTS = kmeans openmp_loop_kmeans #stream_df_kmeans
CFLAGS =  -Wall -g -std=c99 -O3 #-ffast-math -fdump-tree-all -Wno-unused #-Werror
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc

all: $(TESTS)

kmeans: kmeans.c kmeans_common.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

icc_kmeans: kmeans.c kmeans_common.c ../common/common.c
	$(ICC) -std=c99 $^ -o $@ -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

icc_stream_df_kmeans: stream_df_kmeans.c kmeans_common.c stream_df_kmeans_outline_icc.o ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df $(ICC_LIB_DIR)/libirc.a

stream_df_kmeans: stream_df_kmeans.c kmeans_common.c stream_df_kmeans_outline.o ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

icc_stream_df_kmeans_reuse: stream_df_kmeans_reuse.c stream_df_kmeans_reuse_outline_icc.o kmeans_common.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df $(ICC_LIB_DIR)/libirc.a

stream_df_kmeans_reuse: stream_df_kmeans_reuse.c stream_df_kmeans_reuse_outline.o kmeans_common.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

icc_stream_kmeans_from_df: stream_kmeans_from_df.c stream_kmeans_from_df_outline_icc.o kmeans_common.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df $(ICC_LIB_DIR)/libirc.a

stream_kmeans_from_df: stream_kmeans_from_df.c stream_kmeans_from_df_outline.o kmeans_common.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

openmp_loop_kmeans: openmp_loop_kmeans.c kmeans_common.c ../common/common.c
	$(PLAIN_GCC) $^ -o $@ -fopenmp $(CFLAGS)

icc_openmp_loop_kmeans: openmp_loop_kmeans.c kmeans_common.c ../common/common.c
	$(ICC) $^ -o $@ -fopenmp $(CFLAGS)

openmp_loop_kmeans_interleaved: openmp_loop_kmeans_interleaved.c kmeans_common.c ../common/common.c
	$(PLAIN_GCC) $^ -o $@ -fopenmp $(CFLAGS) -I../../install/include/ -L../../install/lib64 -lnuma

icc_openmp_loop_kmeans_interleaved: openmp_loop_kmeans_interleaved.c kmeans_common.c ../common/common.c
	$(ICC) $^ -o $@ -fopenmp $(CFLAGS) -I../../install/include/ -L../../install/lib64 -lnuma

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt *.o \
	icc_kmeans \
	icc_stream_df_kmeans \
	icc_stream_df_kmeans_reuse \
	icc_stream_kmeans_from_df \
	icc_openmp_loop_kmeans \
	icc_openmp_loop_kmeans_interleaved

%_outline.o: %_outline.c
	$(GCC) $(CFLAGS) $(LDFLAGS) -c $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR)

%_outline_icc.o: %_outline.c
	@[ ! -z "$(ICC)" ] || (echo "Could not find ICC" ; exit 1)
	$(ICC) -std=c99 -c $^ -o $@
