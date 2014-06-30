include ../Makefile.config

TESTS = ring_roberts stream_df_ring_roberts stream_ring_roberts_from_df
CFLAGS =  -Wall -g -std=c99 -O3 -Wunused-parameter -ffast-math $(PROFILER_CFLAGS)
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc

all: $(TESTS)

ring_roberts: ring_roberts.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline

stream_%: stream_%.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
