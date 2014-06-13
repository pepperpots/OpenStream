include ../Makefile.config

TESTS = 001_scalar_streams 002_scalar_streams_broadcast \
	003_access_interleaving 004_arrays_of_streams \
	005_dynamic_arrays_of_streams 006_multi_prod_matching \
	007_simple_multi_prod 008_multi_consumer_interleaving \
	009_modular_compilation 010_modular_compilation_2 \
	011_modular_with_arrays_of_streams \
	012_variadic_clauses_with_arrays_of_windows \
	013_stream_selection_arrays \
	014_broadcasting_across_selection_arrays \
	015_nested_streaming \
	016_lastprivate_clause 017_lastprivate_clause_nested \
	018_modular_taskwait 019_streams_of_streams \
	020_inout_reuse

CFLAGS = -fopenmp -Wall -std=c99 -O2 -ffast-math -g $(LD_RPATH_FLAGS) #-fdump-tree-all  #-Werror
LDFLAGS = -L$(LIB_DIR) -lm -rdynamic $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

all: $(TESTS)

.c:
	$(GCC) $(CFLAGS) $(LDFLAGS) -fopenmp $< -o $*

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw a.out *.txt
