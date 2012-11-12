include ../Makefile.config

TESTS = fmradio_seq stream_fmradio
CFLAGS = -Wall -std=c99 -O3 -ffast-math #-fdump-tree-all-all
LDFLAGS = -L$(LIB_DIR) -lm -rdynamic -Wl,-rpath,$(LIB_DIR)

all: $(TESTS)

fmradio_seq: fmradio_seq.c
	$(GCC) $(CFLAGS) $^ -o $@ -fno-inline $(LDFLAGS) # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_fmradio: stream_fmradio.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_LIB_DIR) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
