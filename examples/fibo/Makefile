TESTS = fibo stream_fibo stream_fibo_single_stream stream_recursive_fibo hand_task_fibo raw_fibo

INST_DIR = ../../install/

INSTALL_DIR = ${INST_DIR}
LIB_DIR = ${INSTALL_DIR}/lib64
BIN_DIR = ${INSTALL_DIR}/bin
GCC = ${BIN_DIR}/gcc
LIBWSTREAM_DF_DIR = $(LIB_DIR)
###########################################################################

CFLAGS += -Wall -std=c99 -O3 -ffast-math
LDFLAGS += -L${LIB_DIR} -lm -rdynamic -Wl,-rpath,$(LIB_DIR)

all: $(TESTS)

fibo: fibo.c
	$(GCC) $(CFLAGS) $^ -o $@ -fno-inline $(LDFLAGS) # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_%: LDFLAGS += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df
stream_%: CFLAGS += -fopenmp
stream_%: stream_%.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df

raw_%: LDFLAGS += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df -lpthread
raw_%: raw_%.c
	$(GCC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
hand_%: LDFLAGS += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df -lpthread
hand_%: hand_%.c
	$(GCC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt $(MEM_CST_TESTS)

run:
	./run.sh 40 25

#################################
##       CILK COMPARISON       ##
#################################
CILKC = /media/disk/dev/install/bin/cilkc

cilk_fibo: cilk_fibo.cilk
	$(CILKC) -O2 -g cilk_fibo.cilk -o cilk_fibo #-fdump-tree-all

####################################################

MEM_CST_TESTS = barrier_fibo barrier_fibo_c11 barrier_fibo_dumbc11 barrier_fibo_nofences

mem_cst_test: $(MEM_CST_TESTS)

barrier_%: CFLAGS += -fopenmp
barrier_fibo: LDFLAGS += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df
barrier_fibo_c11: LDFLAGS += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df_c11
barrier_fibo_dumbc11: LDFLAGS += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df_dumbc11
barrier_fibo_nofences: LDFLAGS += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df_nofences
barrier_%: barrier_fibo.c
	$(GCC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

