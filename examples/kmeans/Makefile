include ../Makefile.config

TESTS = kmeans openmp_loop_kmeans stream_df_kmeans
CFLAGS =  -Wall -g -std=c99 -O3 -ffast-math #-fdump-tree-all #-Werror
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc

all: $(TESTS)

kmeans: kmeans.c kmeans_common.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_df_kmeans: stream_df_kmeans.c kmeans_common.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

openmp_loop_kmeans: openmp_loop_kmeans.c kmeans_common.c ../common/common.c
	$(PLAIN_GCC) $^ -o $@ -fopenmp $(CFLAGS)

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
