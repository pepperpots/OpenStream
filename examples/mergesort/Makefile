include ../Makefile.config

TESTS = mergesort stream_df_mergesort
CFLAGS = -Wall -g -std=c99 -O3 -ffast-math
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc

all: $(TESTS)

mergesort: mergesort.c ../common/common.c mergesort.h
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@

stream_df_mergesort: stream_df_mergesort.c ../common/common.c ../common/sync.c mergesort.h
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
