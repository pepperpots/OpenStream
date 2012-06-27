TESTS = 001_scalar_streams 002_scalar_streams_broadcast \
	003_access_interleaving 004_arrays_of_streams \
	005_dynamic_arrays_of_streams 006_multi_prod_matching \
	007_simple_multi_prod 008_multi_consumer_interleaving \
	009_modular_compilation 010_modular_compilation_2 \
	011_modular_with_arrays_of_streams \
	012_variadic_clauses_with_arrays_of_windows \
	013_stream_selection_arrays \
	014_broadcasting_across_selection_arrays \
	015_nested_streaming

INST_DIR = ../../install/

INSTALL_DIR = ${INST_DIR}
LIB_DIR = ${INSTALL_DIR}/lib64
BIN_DIR = ${INSTALL_DIR}/bin
GCC = ${BIN_DIR}/gcc
LIBWSTREAM_DF_DIR = $(LIB_DIR)
###########################################################################

CFLAGS = -fopenmp -Wall -std=c99 -O2 -ffast-math -g -Wl,-rpath,$(INST_DIR)/lib64 #-fdump-tree-all  #-Werror
LDFLAGS = -L${LIB_DIR} -lm -rdynamic -Wl,-rpath,$(LIB_DIR) -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df

all: $(TESTS)

.c:
	$(GCC) $(CFLAGS) $(LDFLAGS) -fopenmp $< -o $*

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw a.out *.txt

