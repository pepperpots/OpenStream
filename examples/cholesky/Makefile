include ../Makefile.config

TESTS = cholesky stream_cholesky block_seq_cholesky
CFLAGS =  -Wall -g -std=c99 -O2  -ffast-math -I$(CBLAS_INC_DIR)
LDFLAGS = -L$(LAPACK_LIB_DIR) -L$(LIB_DIR) -lm -lc -rdynamic # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

cholesky: cholesky.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline -lblas -llapack # Need -fno-inline to prevent GCC from deadcoding when no output.

block_seq_cholesky: block_seq_cholesky.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline -lblas -lcblas -llapack $(LD_RPATH_FLAGS) # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_cholesky: stream_cholesky.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df -lblas -lcblas -llapack $(LD_RPATH_FLAGS)

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
