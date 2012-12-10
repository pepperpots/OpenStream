include ../Makefile.config

TESTS = fmradio_seq stream_fmradio_df
CFLAGS = -Wall -g -std=c99 -O3 -ffast-math #-fdump-tree-all-all
LDFLAGS = -L$(LIB_DIR) -lm

all: $(TESTS)

fmradio_seq: fmradio_seq.c
	$(GCC) $(CFLAGS) $^ -o $@ -fno-inline $(LDFLAGS) # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_%: LDFLAGS += $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df
stream_%: CFLAGS += -fopenmp
stream_%: stream_%.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -lwstream_df

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
