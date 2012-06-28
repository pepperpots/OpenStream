TESTS = cholesky stream_cholesky #par_lapack_cholesky

INST_DIR = ../../install/

INSTALL_DIR = ${INST_DIR}
LIB_DIR = ${INSTALL_DIR}/lib64
BIN_DIR = ${INSTALL_DIR}/bin
GCC = ${BIN_DIR}/gcc
LIBWSTREAM_DF_DIR = $(LIB_DIR)

###########################################################################

CFLAGS =  -Wall -std=c99 -O2  -ffast-math -Wl,-rpath,$(INST_DIR)/lib64 -g  #-fdump-tree-all #-Werror
LDFLAGS = -L/usr/lib/lapack/ -llapack -lblas -L${LIB_DIR} -lm -lc -rdynamic -Wl,-rpath,$(LIB_DIR) # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

cholesky: cholesky.c
	$(GCC) $(CFLAGS) $(LDFLAGS) cholesky.c -o cholesky -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_cholesky: stream_cholesky.c
	$(GCC) $(CFLAGS) $(LDFLAGS) stream_cholesky.c -o stream_cholesky -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df


clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt

run:
	./run.sh 10 8 5

#################################
##       CILK COMPARISON       ##
#################################
CILKC = /media/disk/dev/install/bin/cilkc

cilk_cholesky: cilk_cholesky.cilk
	$(CILKC) -O2 cilk_cholesky.cilk -o cilk_cholesky

