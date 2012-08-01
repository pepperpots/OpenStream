TESTS = seidel stream_seidel topsort-deps_stream_seidel starss_seidel starss_to_stream_seidel openmp_task_seidel openmp_loop_seidel

#INST_DIR = ../../install/
INST_DIR = /media/disk/dev/tstar/open-stream-code/install/

INSTALL_DIR = ${INST_DIR}
LIB_DIR = ${INSTALL_DIR}/lib64
BIN_DIR = ${INSTALL_DIR}/bin
GCC = ${BIN_DIR}/gcc
LIBWSTREAM_DF_DIR = $(LIB_DIR)

PLAIN_GCC = /usr/bin/gcc

STARSS_INSTALL = ../../../../ompss_regions/install
MCC = $(STARSS_INSTALL)/bin/mcc
###########################################################################

CFLAGS =  -Wall -std=c99 -O3  -ffast-math -Wl,-rpath,$(INST_DIR)/lib64 -g  #-fdump-tree-all #-Werror
LDFLAGS = -L${LIB_DIR} -lm -lc -rdynamic -Wl,-rpath,$(LIB_DIR) # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

CFLAGS_ += -Wall -std=c99 -O3 -ffast-math
LDFLAGS_ += -L${LIB_DIR} -lm -rdynamic -Wl,-rpath,$(LIB_DIR)

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

openmp_task_seidel: openmp_task_seidel.c
	$(PLAIN_GCC) $< -o $@ -fopenmp -O3 -ffast-math -std=c99

openmp_loop_seidel: openmp_loop_seidel.c
	$(PLAIN_GCC) $< -o $@ -fopenmp -O3 -ffast-math -std=c99

#.c:
#	$(GCC) $(CFLAGS) $(LDFLAGS) -fopenmp $< -o $* -fdump-tree-all-all

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt $(MEM_CST_TESTS)

run:
	./run.sh 10 8 5

#################################
##       CILK COMPARISON       ##
#################################
CILKC = /media/disk/dev/install/bin/cilkc

cilk_seidel: cilk_seidel.cilk
	$(CILKC) -O2 cilk_seidel.cilk -o cilk_seidel


MEM_CST_TESTS = barrier_seidel barrier_seidel_c11 barrier_seidel_dumbc11 barrier_seidel_nofences

mem_cst_test: $(MEM_CST_TESTS)

barrier_%: CFLAGS_ += -fopenmp
barrier_seidel: LDFLAGS_ += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df
barrier_seidel_c11: LDFLAGS_ += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df_c11
barrier_seidel_dumbc11: LDFLAGS_ += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df_dumbc11
barrier_seidel_nofences: LDFLAGS_ += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df_nofences
barrier_%: barrier_seidel.c
	$(GCC) $(CFLAGS_) $^ -o $@ $(LDFLAGS_)

