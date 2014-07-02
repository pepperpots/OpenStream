include ../Makefile.config

TESTS = cholesky \
	stream_cholesky \
	block_seq_cholesky \
	stream_df_cholesky \
	stream_cholesky_from_df \
	stream_df_cholesky_reuse \
	stream_starss_translation_cholesky \
	stream_df_cholesky_hyb \
	stream_df_cholesky_hyb_reuse

CFLAGS =  -Wall -g -std=c99 -O2  -ffast-math -I$(CBLAS_INC_DIR) $(PROFILER_CFLAGS)
LDFLAGS = -L$(LAPACK_LIB_DIR) -L$(LIB_DIR) -lm -lc -rdynamic # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

cholesky: cholesky.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline -lblas -llapack $(LD_RPATH_FLAGS) # Need -fno-inline to prevent GCC from deadcoding when no output.

block_seq_cholesky: block_seq_cholesky.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline -lblas -lcblas -llapack $(LD_RPATH_FLAGS) # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_%: stream_%.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df -lblas -lcblas -llapack $(LD_RPATH_FLAGS)

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt \
		cilk_cholesky \
		openmp_loop_cholesky \
		openmp_task_cholesky \
		openmp_loop_cholesky_interleaved \
		openmp_task_cholesky_interleaved \
		mkl_cholesky \
		mkl_par_cholesky \
		mkl_block_seq_cholesky \
		mkl_stream_cholesky \
		mkl_stream_cholesky_from_df \
		mkl_stream_df_cholesky \
		mkl_stream_df_cholesky_reuse \
		mkl_stream_df_cholesky_hyb \
		mkl_stream_df_cholesky_hyb_reuse

cilk_cholesky: cilk_cholesky.cilk  ../common/common.c
	$(CILKC) -I$(CBLAS_INC_DIR) -O2 $^ -o $@ -lm -lc -llapack -lblas -lcblas -L$(LAPACK_LIB_DIR) $(LD_RPATH_FLAGS) -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200809L

openmp_%_interleaved: openmp_%.c ../common/common.c ../../libworkstream_df/interleave.c
	$(PLAIN_GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -llapack -lblas -lcblas $(LD_RPATH_FLAGS) -I../../install/include/ -L../../install/lib64 -lnuma

openmp_%: openmp_%.c ../common/common.c
	$(PLAIN_GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -llapack -lblas -lcblas $(LD_RPATH_FLAGS) -I../../install/include/ -L../../install/lib64 -lnuma

# Versions using the Intel Math Kernel Library
CFLAGS_MKL=-I$(MKLROOT)/include -DUSE_MKL -std=c99
LDFLAGS_MKL_SEQ=-Wl,--start-group \
	$(MKLROOT)/lib/intel64/libmkl_intel_lp64.a \
	$(MKLROOT)/lib/intel64/libmkl_core.a \
	$(MKLROOT)/lib/intel64/libmkl_sequential.a \
	-Wl,--end-group \
	-lpthread -lm -ldl

LDFLAGS_MKL_PAR= -Wl,--start-group \
	$(MKLROOT)/lib/intel64/libmkl_intel_lp64.a \
	$(MKLROOT)/lib/intel64/libmkl_core.a \
	$(MKLROOT)/lib/intel64/libmkl_intel_thread.a \
	-Wl,--end-group \
	-L$(ICC_LIB_DIR) \
	-liomp5 -ldl -lpthread -lm -lnuma

mkl_cholesky: cholesky.c ../common/common.c
	@[ ! -z "$(MKLROOT)" ] || (echo "Could not find MKL installation" ; exit 1)
	$(GCC)  $(CFLAGS_MKL) $^ -o $@ -fno-inline $(LDFLAGS_MKL_SEQ)

mkl_par_cholesky: mkl_par_cholesky.c ../common/common.c
	@[ ! -z "$(MKLROOT)" ] || (echo "Could not find MKL installation" ; exit 1)
	$(GCC)  $(CFLAGS_MKL) $^ -o $@ -fno-inline $(LDFLAGS_MKL_PAR)

mkl_block_seq_cholesky: block_seq_cholesky.c ../common/common.c
	@[ ! -z "$(MKLROOT)" ] || (echo "Could not find MKL installation" ; exit 1)
	$(GCC)  $(CFLAGS_MKL) $^ -o $@ -fno-inline $(LDFLAGS_MKL_SEQ)

mkl_stream_%: stream_%.c ../common/common.c ../common/sync.c
	@[ ! -z "$(MKLROOT)" ] || (echo "Could not find MKL installation" ; exit 1)
	$(GCC) $(CFLAGS) $(LDFLAGS) $(CFLAGS_MKL) $^ -o $@ -fopenmp -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df $(LD_RPATH_FLAGS) $(LDFLAGS_MKL_SEQ)
