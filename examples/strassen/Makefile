
clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt $(MEM_CST_TESTS_X86) $(MEM_CST_TESTS_ARM)

#############################################################################

MEM_CST_TESTS_X86 = x86_strassen x86_strassen_c11 x86_strassen_dumbc11 x86_strassen_nofences
MEM_CST_TESTS_DEPS_X86 = barrier_strassen.c ../../libworkstream_df/wstream_df.c ../../libworkstream_df/error.c
mem_cst_test_x86: $(MEM_CST_TESTS_X86)
mem_cst_test_x86: CC_X86 = ../../install/bin/gcc
mem_cst_test_x86: CFLAGS_X86 = $(CFLAGS) -fopenmp -O3 -ffast-math -std=c99 -Wall -Wextra -I../../libworkstream_df/ -D_GNU_SOURCE -fPIC
mem_cst_test_x86: LDFLAGS_X86 = $(LDFLAGS) -static -lm -lrt -lpthread
x86_strassen:
x86_strassen_c11: CFLAGS_X86 += -DUSE_STDATOMIC=1
x86_strassen_dumbc11: CFLAGS_X86 += -DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1
x86_strassen_nofences: CFLAGS_X86 += -DNO_FENCES=1
x86_strass%: $(MEM_CST_TESTS_DEPS_X86)
	$(CC_X86) $(CFLAGS_X86) $^ -o $@ $(LDFLAGS_X86)



MEM_CST_TESTS_ARM = arm_strassen arm_strassen_c11 arm_strassen_dumbc11 arm_strassen_nofences
MEM_CST_TESTS_DEPS_ARM = barrier_strassen.c ../../libworkstream_df/wstream_df_arm.c ../../libworkstream_df/error.c
mem_cst_test_arm: $(MEM_CST_TESTS_ARM)
mem_cst_test_arm: CC_ARM = /home/nhatle/open-stream-code/target.arm/bin/arm-none-linux-gnueabi-gcc
mem_cst_test_arm: CFLAGS_ARM = $(CFLAGS) -fopenmp -D_WSTREAM_DF_NUM_THREADS=4 -O3 -ffast-math -std=c99 -Wall -Wextra -march=armv7-a -DNO_TEST_SETAFFINITY=1 -I../../libworkstream_df/ -fPIC
mem_cst_test_arm: LDFLAGS_ARM = $(LDFLAGS) -static -lm -lrt -lpthread
arm_strassen:
arm_strassen_c11: CFLAGS_ARM += -DUSE_STDATOMIC=1
arm_strassen_dumbc11: CFLAGS_ARM += -DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1
arm_strassen_nofences: CFLAGS_ARM += -DNO_FENCES=1
arm_strass%: $(MEM_CST_TESTS_DEPS_ARM)
	$(CC_ARM) $(CFLAGS_ARM) $^ -o $@ $(LDFLAGS_ARM)


