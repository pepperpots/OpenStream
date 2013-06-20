include ../Makefile.config

TESTS = cholesky stream_cholesky block_seq_cholesky stream_df_cholesky stream_starss_translation_cholesky
CFLAGS =  -Wall -g -std=c99 -O2  -ffast-math -I$(CBLAS_INC_DIR) $(PROFILER_CFLAGS)
LDFLAGS = -L$(LAPACK_LIB_DIR) -L$(LIB_DIR) -lm -lc -rdynamic # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

cholesky: cholesky.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline -lblas -llapack $(LD_RPATH_FLAGS) # Need -fno-inline to prevent GCC from deadcoding when no output.

block_seq_cholesky: block_seq_cholesky.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline -lblas -lcblas -llapack $(LD_RPATH_FLAGS) # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_%: stream_%.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df -lblas -lcblas -llapack $(LD_RPATH_FLAGS)

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt cilk_cholesky openmp_loop_cholesky openmp_task_cholesky

cilk_cholesky: cilk_cholesky.cilk  ../common/common.c ../common/sync.c
	$(CILKC) -I$(CBLAS_INC_DIR) -O2 $^ -o $@ -lm -lc -llapack -lblas -lcblas -L$(LAPACK_LIB_DIR) $(LD_RPATH_FLAGS) -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200809L

openmp_%: openmp_%.c ../common/common.c ../common/sync.c
	$(PLAIN_GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -llapack -lblas -lcblas $(LD_RPATH_FLAGS)
