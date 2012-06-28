TESTS = fibo stream_fibo stream_fibo_single_stream stream_recursive_fibo

INST_DIR = ../../install/

INSTALL_DIR = ${INST_DIR}
LIB_DIR = ${INSTALL_DIR}/lib64
BIN_DIR = ${INSTALL_DIR}/bin
GCC = ${BIN_DIR}/gcc
LIBWSTREAM_DF_DIR = $(LIB_DIR)
###########################################################################

CFLAGS =  -Wall -std=c99 -O2 -Wl,-rpath,$(INST_DIR)/lib64 -g #-fdump-tree-all-all #-Werror
LDFLAGS = -L${INST_DIR}/lib -Wl,-rpath,$(INST_DIR)/lib -L${LIB_DIR} -lm -rdynamic -Wl,-rpath,$(LIB_DIR) # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

fibo: fibo.c
	$(GCC) $(CFLAGS) $(LDFLAGS) fibo.c -o fibo -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_fibo: stream_fibo.c
	$(GCC) $(CFLAGS) $(LDFLAGS) stream_fibo.c -o stream_fibo -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df

stream_recursive_fibo: stream_recursive_fibo.c
	$(GCC) $(CFLAGS) $(LDFLAGS) stream_recursive_fibo.c -o stream_recursive_fibo -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df

stream_fibo_single_stream: stream_fibo_single_stream.c
	$(GCC) $(CFLAGS) $(LDFLAGS) stream_fibo_single_stream.c -o stream_fibo_single_stream -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt

run:
	./run.sh 40 25

#################################
##       CILK COMPARISON       ##
#################################
CILKC = /media/disk/dev/install/bin/cilkc

cilk_fibo: cilk_fibo.cilk
	$(CILKC) -O2 -g cilk_fibo.cilk -o cilk_fibo #-fdump-tree-all
