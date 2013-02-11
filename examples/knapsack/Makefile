include ../Makefile.config

MEM_CST_TESTS_X86 = x86_knapsack x86_knapsack_c11 x86_knapsack_dumbc11 x86_knapsack_nofences
CFLAGS_X86 = $(CFLAGS) -fopenmp -O3 -ffast-math -std=c99 -Wall -Wextra -I../../libworkstream_df/ -D_GNU_SOURCE -fPIC
LDFLAGS_X86 = $(LDFLAGS) -lm

all: $(MEM_CST_TESTS_X86)

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt $(MEM_CST_TESTS_X86)

MEM_CST_TESTS_DEPS_X86 = barrier_knapsack.c\
	../../libworkstream_df/wstream_df.c \
	../../libworkstream_df/error.c \
	../../libworkstream_df/trace.c \
	../../libworkstream_df/fibers.c \
	../../libworkstream_df/papi-defs.c \
	../../libworkstream_df/profiling.c \
	../common/common.c
x86_knapsack: CFLAGS_X86 += -D_GNU_SOURCE
x86_knapsack_c11: CFLAGS_X86 += -DUSE_STDATOMIC=1
x86_knapsack_dumbc11: CFLAGS_X86 += -DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1
x86_knapsack_nofences: CFLAGS_X86 += -DNO_FENCES=1
x86_knap%: $(MEM_CST_TESTS_DEPS_X86)
	$(CC_X86) $(CFLAGS_X86) $^ -o $@ $(LDFLAGS_X86)
