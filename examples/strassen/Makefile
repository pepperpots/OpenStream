include ../Makefile.config

MEM_CST_TESTS_X86 = x86_strassen x86_strassen_c11 x86_strassen_dumbc11 x86_strassen_nofences
CFLAGS_X86 = $(CFLAGS) -fopenmp -O3 -ffast-math -std=c99 -Wall -Wextra -I../../libworkstream_df/ -D_GNU_SOURCE -fPIC
LDFLAGS_X86 = $(LDFLAGS) -lm

ifneq ($(wildcard ../../install/bin/papi_avail),)
PAPI_DIR = ../../install/
CFLAGS_X86 += -I$(PAPI_DIR)/include
LDFLAGS_X86 += -L$(PAPI_DIR)/lib -lpapi
endif

all: $(MEM_CST_TESTS_X86)

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt $(MEM_CST_TESTS_X86)

../../libworkstream_df/trace_file.c:
	cd `dirname $@` ; make `basename $@`

MEM_CST_TESTS_DEPS_X86 = barrier_strassen.c \
	../../libworkstream_df/wstream_df.c \
	../../libworkstream_df/error.c \
	../../libworkstream_df/trace.c \
	../../libworkstream_df/fibers.c \
	../../libworkstream_df/profiling.c \
	../../libworkstream_df/work_distribution.c \
	../../libworkstream_df/trace_file.c \
	../common/common.c
x86_strassen: CFLAGS_X86 += -D_GNU_SOURCE
x86_strassen_c11: CFLAGS_X86 += -DUSE_STDATOMIC=1
x86_strassen_dumbc11: CFLAGS_X86 += -DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1
x86_strassen_nofences: CFLAGS_X86 += -DNO_FENCES=1
x86_strass%: $(MEM_CST_TESTS_DEPS_X86)
	$(CC_X86) $(CFLAGS_X86) $^ -o $@ $(LDFLAGS_X86)
