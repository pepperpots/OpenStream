include ../Makefile.config

TESTS = seidel stream_seidel topsort-deps_stream_seidel starss_to_stream_seidel openmp_task_seidel openmp_loop_seidel
CFLAGS =  -Wall -g -std=c99 -O2  -ffast-math  #-fdump-tree-all #-Werror
LDFLAGS = -L${LIB_DIR} -lm -lc -rdynamic -Wl,-rpath,$(LIB_DIR) # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

ifneq ($(MCC),)
TESTS += starss_seidel
endif

all: $(TESTS)

seidel: seidel.c
	$(GCC) $(CFLAGS) $(LDFLAGS) seidel.c -o seidel -fno-inline # Need -fno-inline to prevent GCC from deadcoding when no output.

stream_seidel: stream_seidel.c
	$(GCC) $(CFLAGS) $(LDFLAGS) stream_seidel.c -o stream_seidel -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_LIB_DIR) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

topsort-deps_stream_seidel: topsort-deps_stream_seidel.c
	$(GCC) $(CFLAGS) $(LDFLAGS) topsort-deps_stream_seidel.c -o topsort-deps_stream_seidel -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_LIB_DIR) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

starss_to_stream_seidel: starss_to_stream_seidel.c
	$(GCC) $(CFLAGS) $(LDFLAGS) starss_to_stream_seidel.c -o starss_to_stream_seidel -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_LIB_DIR) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

starss_seidel: starss_seidel.c
	$(MCC) --ompss starss_seidel.c -o starss_seidel  -O3 -g

openmp_task_seidel: openmp_task_seidel.c
	$(PLAIN_GCC) $< -o $@ -fopenmp -O3 -ffast-math -std=c99

openmp_loop_seidel: openmp_loop_seidel.c
	$(PLAIN_GCC) $< -o $@ -fopenmp -O3 -ffast-math -std=c99

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt $(MEM_CST_TESTS_X86) $(MEM_CST_TESTS_ARM)

cilk_seidel: cilk_seidel.cilk
	$(CILKC) -O2 cilk_seidel.cilk -o cilk_seidel
