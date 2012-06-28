TESTS = seidel stream_seidel topsort-deps_stream_seidel starss_seidel starss_to_stream_seidel

INST_DIR = ../../install/

INSTALL_DIR = ${INST_DIR}
LIB_DIR = ${INSTALL_DIR}/lib64
BIN_DIR = ${INSTALL_DIR}/bin
GCC = ${BIN_DIR}/gcc
LIBWSTREAM_DF_DIR = $(LIB_DIR)

STARSS_INSTALL = ../../../../ompss_regions/install
MCC = $(STARSS_INSTALL)/bin/mcc
###########################################################################

CFLAGS =  -Wall -std=c99 -O2  -ffast-math -Wl,-rpath,$(INST_DIR)/lib64 -g  #-fdump-tree-all #-Werror
LDFLAGS = -L${LIB_DIR} -lm -lc -rdynamic -Wl,-rpath,$(LIB_DIR) # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

seidel: seidel.c
	$(GCC) $(CFLAGS) $(LDFLAGS) seidel.c -o seidel -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_seidel: stream_seidel.c
	$(GCC) $(CFLAGS) $(LDFLAGS) stream_seidel.c -o stream_seidel -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df

topsort-deps_stream_seidel: topsort-deps_stream_seidel.c
	$(GCC) $(CFLAGS) $(LDFLAGS) topsort-deps_stream_seidel.c -o topsort-deps_stream_seidel -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df

starss_to_stream_seidel: starss_to_stream_seidel.c
	$(GCC) $(CFLAGS) $(LDFLAGS) starss_to_stream_seidel.c -o starss_to_stream_seidel -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df

starss_seidel: starss_seidel.c
	$(MCC) --ompss starss_seidel.c -o starss_seidel  -O3 -g

#.c:
#	$(GCC) $(CFLAGS) $(LDFLAGS) -fopenmp $< -o $* -fdump-tree-all-all

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt

run:
	./run.sh 10 8 5

#################################
##       CILK COMPARISON       ##
#################################
CILKC = /media/disk/dev/install/bin/cilkc

cilk_seidel: cilk_seidel.cilk
	$(CILKC) -O2 cilk_seidel.cilk -o cilk_seidel

