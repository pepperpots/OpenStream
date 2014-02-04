include ../../Makefile.config

GENERATORS=line_dag cross_dag
TESTS = rdag stream_df_rdag $(addprefix generators/,$(GENERATORS))
CFLAGS =  -Wall -g -std=c99 -O2  -ffast-math $(PROFILER_CFLAGS) # -fdump-tree-all #-Werror
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

rdag: rdag.c rdag-common.c hash.c hash.h rand.c ../../common/common.c ../../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) -g -std=c99 -o $@ $^

generators/%: generators/%.c rand.c
	$(GCC) $(CFLAGS) $(LDFLAGS) -g -std=c99 -o $@ $^

stream_df_rdag: stream_df_rdag.c rand.c rdag-common.c hash.h hash.c ../../common/common.c ../../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
