include ../Makefile.config

TESTS = sparse_lu stream_sparse_lu stream_df_sparse_lu
CFLAGS =  -Wall -g -std=c99 -O2 $(PROFILER_CFLAGS) -ffast-math  #-fdump-tree-ompexp-all #-Werror
LDFLAGS = -L${LIB_DIR} -lm -lc -rdynamic # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

ifneq ($(MCC),)
TESTS += starss_sparse_lu
endif

all: $(TESTS)

sparse_lu: sparse_lu.c ../common/common.c ../common/sync.c
	$(GCC) $(DFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_sparse_lu: stream_sparse_lu.c ../common/common.c ../common/sync.c
	$(GCC) $(DFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline -fopenmp $(LD_RPATH_FLAGS) -L${LIBWSTREAM_DF_LIB_DIR} -lwstream_df

stream_df_sparse_lu: stream_df_sparse_lu.c ../common/common.c ../common/sync.c
	$(GCC) $(DFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline -fopenmp $(LD_RPATH_FLAGS) -L${LIBWSTREAM_DF_LIB_DIR} -lwstream_df

starss_sparse_lu: starss_sparse_lu.c ../common/common.c ../common/sync.c
	$(MCC) $(DFLAGS) --ompss s$^ -o $@ -O3 -g

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt

run:
	./run.sh 10 8 5

cilk_sparse_lu: cilk_sparse_lu.cilk ../common/common.c ../common/sync.c
	$(CILKC) -O2 $^ -o $@
