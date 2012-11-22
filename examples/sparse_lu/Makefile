include ../Makefile.config

TESTS = sparse_lu stream_sparse_lu
CFLAGS =  -Wall -g -std=c99 -O2  -ffast-math  #-fdump-tree-ompexp-all #-Werror
LDFLAGS = -L${LIB_DIR} -lm -lc -rdynamic -Wl,-rpath,$(LIB_DIR) # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

ifneq ($(MCC),)
TESTS += starss_sparse_lu
endif

all: $(TESTS)

sparse_lu: sparse_lu.c
	$(GCC) $(DFLAGS) $(CFLAGS) $(LDFLAGS) sparse_lu.c -o sparse_lu -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_sparse_lu: stream_sparse_lu.c
	$(GCC) $(DFLAGS) $(CFLAGS) $(LDFLAGS) stream_sparse_lu.c -o stream_sparse_lu  -fno-inline -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_LIB_DIR) -L${LIBWSTREAM_DF_LIB_DIR} -lwstream_df

starss_sparse_lu: starss_sparse_lu.c
	$(MCC) $(DFLAGS) --ompss starss_sparse_lu.c -o starss_sparse_lu  -O3 -g

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt

run:
	./run.sh 10 8 5

cilk_sparse_lu: cilk_sparse_lu.cilk
	$(CILKC) -O2 cilk_sparse_lu.cilk -o cilk_sparse_lu

