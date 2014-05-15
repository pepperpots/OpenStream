include ../Makefile.config

TESTS = bitonic stream_df_bitonic stream_bitonic_from_df
CFLAGS = -Wall -g -std=c99 -O3 -ffast-math
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc

all: $(TESTS)

bitonic: bitonic.c ../common/common.c bitonic.h
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@

stream_df_bitonic: stream_df_bitonic.c ../common/common.c bitonic.h
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

stream_df_bitonic_reuse: stream_df_bitonic_reuse.c ../common/common.c bitonic.h
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
