TESTS = fibo stream_fibo stream_fibo_single_stream stream_recursive_fibo hand_task_fibo raw_fibo

#INST_DIR = ../../install/
INST_DIR = /media/disk/dev/tstar/open-stream-code/install/

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
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt $(MEM_CST_TESTS_X86) $(MEM_CST_TESTS_ARM)

run:
	./run.sh 40 25

#################################
##       CILK COMPARISON       ##
#################################
CILKC = /media/disk/dev/install/bin/cilkc

cilk_fibo: cilk_fibo.cilk
	$(CILKC) -O2 -g cilk_fibo.cilk -o cilk_fibo #-fdump-tree-all

#############################################################################

MEM_CST_TESTS_X86 = x86_fibo x86_fibo_c11 x86_fibo_dumbc11 x86_fibo_nofences
MEM_CST_TESTS_DEPS_X86 = barrier_fibo.c ../../libworkstream_df/wstream_df.c ../../libworkstream_df/error.c
mem_cst_test_x86: $(MEM_CST_TESTS_X86)
mem_cst_test_x86: CC_X86 = ../../install/bin/gcc
mem_cst_test_x86: CFLAGS_X86 = $(CFLAGS) -fopenmp -O3 -ffast-math -std=c99 -Wall -Wextra -I../../libworkstream_df/ -D_GNU_SOURCE -fPIC
mem_cst_test_x86: LDFLAGS_X86 = $(LDFLAGS) -static -lm -lrt -lpthread
x86_fibo:
x86_fibo_c11: CFLAGS_X86 += -DUSE_STDATOMIC=1
x86_fibo_dumbc11: CFLAGS_X86 += -DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1
x86_fibo_nofences: CFLAGS_X86 += -DNO_FENCES=1
x86_fib%: $(MEM_CST_TESTS_DEPS_X86)
	$(CC_X86) $(CFLAGS_X86) $^ -o $@ $(LDFLAGS_X86)



MEM_CST_TESTS_ARM = arm_fibo arm_fibo_c11 arm_fibo_dumbc11 arm_fibo_nofences
MEM_CST_TESTS_DEPS_ARM = barrier_fibo.c ../../libworkstream_df/wstream_df_arm.c ../../libworkstream_df/error.c
mem_cst_test_arm: $(MEM_CST_TESTS_ARM)
mem_cst_test_arm: CC_ARM = /home/nhatle/open-stream-code/target.arm/bin/arm-none-linux-gnueabi-gcc
mem_cst_test_arm: CFLAGS_ARM = $(CFLAGS) -fopenmp -D_WSTREAM_DF_NUM_THREADS=4 -O3 -ffast-math -std=c99 -Wall -Wextra -march=armv7-a -DNO_TEST_SETAFFINITY=1 -I../../libworkstream_df/ -fPIC
mem_cst_test_arm: LDFLAGS_ARM = $(LDFLAGS) -static -lm -lrt -lpthread
arm_fibo:
arm_fibo_c11: CFLAGS_ARM += -DUSE_STDATOMIC=1
arm_fibo_dumbc11: CFLAGS_ARM += -DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1
arm_fibo_nofences: CFLAGS_ARM += -DNO_FENCES=1
arm_fib%: $(MEM_CST_TESTS_DEPS_ARM)
	$(CC_ARM) $(CFLAGS_ARM) $^ -o $@ $(LDFLAGS_ARM)


