TESTS = seq_fftw radix_fft_stream_pure barrier_fft

#INST_DIR = ../../install/
INST_DIR = /media/disk/dev/tstar/open-stream-code/install/
FFTW_INST_DIR = /media/disk/dev/install

INSTALL_DIR = ${INST_DIR}
LIB_DIR = ${INSTALL_DIR}/lib64
BIN_DIR = ${INSTALL_DIR}/bin
GCC = ${BIN_DIR}/gcc
LIBWSTREAM_DF_DIR = $(LIB_DIR)
###########################################################################

CFLAGS =  -Wall -std=c99 -O2  -ffast-math -Wl,-rpath,$(INST_DIR)/lib64 -g  #-w -fdump-tree-all-all #-Werror
LDFLAGS = -L${LIB_DIR} -lm -L/usr/lib/lapack/ -llapack -rdynamic -Wl,-rpath,$(LIB_DIR) -L${FFTW_INST_DIR}/lib/ -lfftw3 -I$(FFTW_INST_DIR)/include -Wl,-rpath,$(FFTW_INST_DIR)/lib # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

seq_fftw: seq_fftw.c
	$(GCC) $(CFLAGS) $(LDFLAGS) seq_fftw.c -o seq_fftw -fno-inline

radix_fft_stream_pure: radix_fft_stream_pure.c
	$(GCC) $(CFLAGS) $(LDFLAGS) radix_fft_stream_pure.c -o radix_fft_stream_pure -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df

#.c:
#	$(GCC) $(CFLAGS) $(LDFLAGS) -fopenmp $< -o $* -fdump-tree-all-all

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt $(MEM_CST_TESTS_X86) $(MEM_CST_TESTS_ARM)

run:
	./run.sh 19 2 50



#############################################################################

MEM_CST_TESTS_X86 = x86_fft x86_fft_c11 x86_fft_dumbc11 x86_fft_nofences
MEM_CST_TESTS_DEPS_X86 = barrier_fft.c ../../libworkstream_df/wstream_df.c ../../libworkstream_df/error.c
mem_cst_test_x86: $(MEM_CST_TESTS_X86)
mem_cst_test_x86: CC_X86 = ../../install/bin/gcc
mem_cst_test_x86: CFLAGS_X86 = -fopenmp -O3 -ffast-math -std=c99 -Wall -Wextra -I../../libworkstream_df/ -D_GNU_SOURCE -fPIC
mem_cst_test_x86: LDFLAGS_X86 = -static -lm -lrt -lpthread
x86_fft:
x86_fft_c11: CFLAGS_X86 += -DUSE_STDATOMIC=1
x86_fft_dumbc11: CFLAGS_X86 += -DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1
x86_fft_nofences: CFLAGS_X86 += -DNO_FENCES=1
x86_ff%: $(MEM_CST_TESTS_DEPS_X86)
	$(CC_X86) $(CFLAGS_X86) $^ -o $@ $(LDFLAGS_X86)



MEM_CST_TESTS_ARM = arm_fft arm_fft_c11 arm_fft_dumbc11 arm_fft_nofences
MEM_CST_TESTS_DEPS_ARM = barrier_fft.c ../../libworkstream_df/wstream_df_arm.c ../../libworkstream_df/error.c
mem_cst_test_arm: $(MEM_CST_TESTS_ARM)
mem_cst_test_arm: CC_ARM = /home/nhatle/open-stream-code/target.arm/bin/arm-none-linux-gnueabi-gcc
mem_cst_test_arm: CFLAGS_ARM = -fopenmp -D_WSTREAM_DF_NUM_THREADS=4 -O3 -ffast-math -std=c99 -Wall -Wextra -march=armv7-a -DNO_TEST_SETAFFINITY=1 -I../../libworkstream_df/ -fPIC
mem_cst_test_arm: LDFLAGS_ARM = -static -lm -lrt -lpthread
arm_fft:
arm_fft_c11: CFLAGS_ARM += -DUSE_STDATOMIC=1
arm_fft_dumbc11: CFLAGS_ARM += -DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1
arm_fft_nofences: CFLAGS_ARM += -DNO_FENCES=1
arm_ff%: $(MEM_CST_TESTS_DEPS_ARM)
	$(CC_ARM) $(CFLAGS_ARM) $^ -o $@ $(LDFLAGS_ARM)

