include ../Makefile.config

TESTS = fibo stream_fibo stream_fibo_single_stream stream_recursive_fibo barrier_fibo
CFLAGS = -Wall -std=c99 -O3 -ffast-math
LDFLAGS = -L$(LIB_DIR) -lm -rdynamic

all: $(TESTS)

fibo: fibo.c ../common/common.c
	$(GCC) $(CFLAGS) $^ -o $@ -fno-inline $(LDFLAGS) # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_%: LDFLAGS += $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df
stream_%: CFLAGS += -fopenmp
stream_%: stream_%.c ../common/sync.c ../common/common.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_DIR) -lwstream_df

barrier_%: LDFLAGS += -L$(LIBWSTREAM_DF_LIB_DIR) $(LD_RPATH_FLAGS) -lwstream_df
barrier_%: CFLAGS += -fopenmp
barrier_%: barrier_%.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp  $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_DIR) -lwstream_df

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt

cilk_fibo: cilk_fibo.cilk ../common/common.c
	$(CILKC) -O2 -g $^ -o $@ -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200809L #-fdump-tree-all
