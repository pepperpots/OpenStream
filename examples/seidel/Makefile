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

#CFLAGS =  -Wall -std=c99 -O3  -ffast-math -Wl,-rpath,$(INST_DIR)/lib64 -g  #-fdump-tree-all #-Werror
#LDFLAGS = -L${LIB_DIR} -lm -lc -rdynamic -Wl,-rpath,$(LIB_DIR) # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

#CFLAGS_ += -Wall -std=c99 -O3 -ffast-math
#LDFLAGS_ += -L${LIB_DIR} -lm -rdynamic -Wl,-rpath,$(LIB_DIR)

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
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt $(MEM_CST_TESTS_X86) $(MEM_CST_TESTS_ARM)

run:
	./run.sh 10 8 5

#################################
##       CILK COMPARISON       ##
#################################
CILKC = /media/disk/dev/install/bin/cilkc

cilk_seidel: cilk_seidel.cilk
	$(CILKC) -O2 cilk_seidel.cilk -o cilk_seidel





#############################################################################

MEM_CST_TESTS_X86 = x86_seidel x86_seidel_c11 x86_seidel_dumbc11 x86_seidel_nofences
MEM_CST_TESTS_DEPS_X86 = barrier_seidel.c ../../libworkstream_df/wstream_df.c ../../libworkstream_df/error.c
mem_cst_test_x86: $(MEM_CST_TESTS_X86)
mem_cst_test_x86: CC_X86 = ../../install/bin/gcc
mem_cst_test_x86: CFLAGS_X86 = $(CFLAGS) -fopenmp -O3 -ffast-math -std=c99 -Wall -Wextra -I../../libworkstream_df/ -D_GNU_SOURCE -fPIC
mem_cst_test_x86: LDFLAGS_X86 = $(LDFLAGS) -static -lm -lrt -lpthread
x86_seidel:
x86_seidel_c11: CFLAGS_X86 += -DUSE_STDATOMIC=1
x86_seidel_dumbc11: CFLAGS_X86 += -DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1
x86_seidel_nofences: CFLAGS_X86 += -DNO_FENCES=1
x86_seid%: $(MEM_CST_TESTS_DEPS_X86)
	$(CC_X86) $(CFLAGS_X86) $^ -o $@ $(LDFLAGS_X86)



MEM_CST_TESTS_ARM = arm_seidel arm_seidel_c11 arm_seidel_dumbc11 arm_seidel_nofences
MEM_CST_TESTS_DEPS_ARM = barrier_seidel.c ../../libworkstream_df/wstream_df_arm.c ../../libworkstream_df/error.c
mem_cst_test_arm: $(MEM_CST_TESTS_ARM)
mem_cst_test_arm: CC_ARM = /home/nhatle/open-stream-code/target.arm/bin/arm-none-linux-gnueabi-gcc
mem_cst_test_arm: CFLAGS_ARM = $(CFLAGS) -fopenmp -D_WSTREAM_DF_NUM_THREADS=4 -O3 -ffast-math -std=c99 -Wall -Wextra -march=armv7-a -DNO_TEST_SETAFFINITY=1 -I../../libworkstream_df/ -fPIC
mem_cst_test_arm: LDFLAGS_ARM = $(LDFLAGS) -static -lm -lrt -lpthread
arm_seidel:
arm_seidel_c11: CFLAGS_ARM += -DUSE_STDATOMIC=1
arm_seidel_dumbc11: CFLAGS_ARM += -DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1
arm_seidel_nofences: CFLAGS_ARM += -DNO_FENCES=1
arm_seid%: $(MEM_CST_TESTS_DEPS_ARM)
	$(CC_ARM) $(CFLAGS_ARM) $^ -o $@ $(LDFLAGS_ARM)


