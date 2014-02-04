include ../../Makefile.config

GENERATOR_NAMES=line_dag cross_dag fork_join
GENERATORS=$(addprefix generators/,$(GENERATOR_NAMES))

TESTS = read_dag stream_df_rdag $(GENERATORS)
CFLAGS =  -Wall -g -std=c99 -O2  -ffast-math $(PROFILER_CFLAGS) # -fdump-tree-all #-Werror
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

read_dag: read_dag.c rdag-common.c hash.c hash.h rand.c ../../common/common.c ../../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) -g -std=c99 -o $@ $^

generators/%: generators/%.c rand.c
	$(GCC) $(CFLAGS) $(LDFLAGS) -g -std=c99 -o $@ $^

stream_df_rdag: stream_df_rdag.c rand.c rdag-common.c hash.h hash.c ../../common/common.c ../../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt

input-data: $(GENERATORS)
	./generators/line_dag -l 64 -n 3000 -w 256000 > input/line-l64-n3k-0x-w256k.dag
	./generators/line_dag -l 64 -i 300 -a 300 -n 3000 -w 256000 > input/line-l64-n3k-300x-w256k.dag
	./generators/line_dag -l 128 -n 3000 -w 256000 > input/line-l128-n3k-0x-w256k.dag
	./generators/line_dag -l 128 -i 300 -a 300 -n 3000 -w 256000 > input/line-l128-n3k-300x-w256k.dag

	./generators/cross_dag -l 64 -n 3000 -w 256000 > input/cross-l64-n3k-w256k.dag
	./generators/cross_dag -l 128 -n 3000 -w 256000 > input/cross-l128-n3k-w256k.dag