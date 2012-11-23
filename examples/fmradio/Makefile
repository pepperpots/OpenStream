include ../Makefile.config

TESTS = fmradio_seq stream_fmradio
CFLAGS = -Wall -g -std=c99 -O3 -ffast-math #-fdump-tree-all-all
LDFLAGS = -L$(LIB_DIR) -lm

all: $(TESTS)

fmradio_seq: fmradio_seq.c
	$(GCC) $(CFLAGS) $^ -o $@ -fno-inline $(LDFLAGS) # Need -fno-inline to prevent GCC from deadcoding when no output.
	touch fmradio_seq.raw fmradio_seq.txt

stream_fmradio: stream_fmradio.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -L$(LIBWSTREAM_DF_LIB_DIR)  $(LD_RPATH_FLAGS) -lwstream_df
	touch stream_fmradio.raw stream_fmradio.txt

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
