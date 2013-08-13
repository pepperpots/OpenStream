include ../Makefile.config

TESTS = kmeans
CFLAGS =  -Wall -g -std=c99 -O3  -ffast-math $(PROFILER_CFLAGS)  #-fdump-tree-all #-Werror
LDFLAGS = -L${LIB_DIR} -rdynamic -lm -lc

all: $(TESTS)

kmeans: kmeans.c ../common/common.c ../common/sync.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt
