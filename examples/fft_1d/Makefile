include ../Makefile.config

TESTS = seq_fftw radix_fft_stream_pure
CFLAGS =  -Wall -g -std=c99 -O2  -ffast-math -I$(FFTW_INC_DIR)  #-w -fdump-tree-all-all #-Werror
LDFLAGS = -L$(LIB_DIR) -L$(BLAS_LIB_DIR) -L$(LAPACK_LIB_DIR) -lm -lblas -llapack -rdynamic -L$(FFTW_LIB_DIR) -lfftw3 # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

seq_fftw: seq_fftw.c ../common/common.c
	$(GCC) $(CFLAGS) $^ -o $@ -fno-inline $(LDFLAGS)

radix_fft_stream_pure: radix_fft_stream_pure.c ../common/common.c
	$(GCC) $(DEFAULT_OPENSTREAM_CFLAGS) $(CFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df $(LDFLAGS)

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
