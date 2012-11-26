INST_DIR = ../../gcc/install_omp

INSTALL_DIR = ${INST_DIR}
LIB_DIR = ${INSTALL_DIR}/lib64
BIN_DIR = ${INSTALL_DIR}/bin
GCC = ${BIN_DIR}/gcc
LIBWSTREAM_DF_DIR = $(LIB_DIR)

###########################################################################
TEST=stream_bzip2 bzip2
CFLAGS =  -Wall -std=c99  -ffast-math -Wl,-rpath,$(INST_DIR)/lib64 -g  -DTIMING_OUTPUT#-fdump-tree-all #-Werror
LDFLAGS = -L${LIB_DIR} -lm -lc -rdynamic -Wl,-rpath,$(LIB_DIR)

CFLAGS_DEBUG = -Wall -g

all: $(TEST)
bzip2:bzip2.c spec.c common.h
	gcc $(CFLAGS_DEBUG) -o $@ $^
stream_bzip2: stream_spec.c stream_bzip2.c common.h
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df
run: stream_bzip2 bzip2
	./run.sh
clean:
	rm out.*
	rm $(TEST)