include ../Makefile.config

TESTS = seq_fftw radix_fft_stream_pure barrier_fft
CFLAGS =  -Wall -std=c99 -O2  -ffast-math -Wl,-rpath,$(LIB_DIR) -g  #-w -fdump-tree-all-all #-Werror
LDFLAGS = -L$(LIB_DIR) -L$(BLAS_LIB_DIR) -L$(LAPACK_LIB_DIR) -lblas -llapack -rdynamic -Wl,-rpath,$(LIB_DIR) -L$(FFTW_LIB_DIR) -lfftw3 -I$(FFTW_INC_DIR) -Wl,-rpath,$(FFTW_LIB_DIR) # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

seq_fftw: seq_fftw.c
	$(GCC) $(CFLAGS) seq_fftw.c -o seq_fftw -fno-inline $(LDFLAGS)

radix_fft_stream_pure: radix_fft_stream_pure.c
	$(GCC) $(CFLAGS) radix_fft_stream_pure.c -o radix_fft_stream_pure -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_LIB_DIR) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df $(LDFLAGS)

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
