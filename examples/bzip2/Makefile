include ../Makefile.config

TESTS=stream_bzip2 bzip2

CFLAGS =  -Wall -g -std=c99 -O3  -ffast-math  #-fdump-tree-all #-Werror
LDFLAGS = -L${LIB_DIR} -lm -lc -rdynamic -Wl,-rpath,$(LIB_DIR) # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

CFLAGS_DEBUG = -Wall -g

all: $(TESTS)
bzip2:bzip2.c spec.c common.h
	$(GCC) $(CFLAGS) $(LDFLAGS) -o $@ $^
stream_bzip2: stream_spec.c stream_bzip2.c common.h
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_LIB_DIR) -L${LIBWSTREAM_DF_LIB_DIR} -lwstream_df
run: stream_bzip2 bzip2
	./run.sh
clean:
	rm out.*
	rm $(TEST) *~ 