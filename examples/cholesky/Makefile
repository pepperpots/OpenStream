include ../Makefile.config

TESTS = cholesky stream_cholesky stream_starss_translation_cholesky
CFLAGS =  -Wall -g -std=c99 -O2  -ffast-math -I$(CBLAS_INC_DIR)
LDFLAGS = -L$(LAPACK_LIB_DIR) -L$(LIB_DIR) -lm -lc -rdynamic -Wl,-rpath,$(LIB_DIR) -Wl,-rpath,$(LAPACK_LIB_DIR) # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

cholesky: cholesky.c
	$(GCC) $(CFLAGS) $(LDFLAGS) cholesky.c -o cholesky -fno-inline -lblas -llapack # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_%: stream_%.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_LIB_DIR) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df -lblas -lcblas -llapack


clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
