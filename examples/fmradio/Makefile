include ../Makefile.config

TESTS = fmradio_seq stream_fmradio
CFLAGS = -Wall -g -std=c99 -O3 -ffast-math #-fdump-tree-all-all
LDFLAGS = -L$(LIB_DIR) -lm -rdynamic -Wl,-rpath,$(LIB_DIR)

all: $(TESTS)

fmradio_seq: fmradio_seq.c
	$(GCC) $(CFLAGS) $^ -o $@ -fno-inline $(LDFLAGS) # Need -fno-inline to prevent GCC from deadcoding when no output.
	touch fmradio_seq.raw fmradio_seq.txt

stream_fmradio: stream_fmradio.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_LIB_DIR) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df
	touch stream_fmradio.raw stream_fmradio.txt

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
