include ../../examples/Makefile.config
CFLAGS=-Wall -g -std=c99 -O2
LDFLAGS=-L${LIB_DIR} -rdynamic -lm -lc

TIMEOUT_SECONDS=2

all: test

test: test.c
	$(GCC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -fopenmp $(LD_RPATH_FLAGS) -L$(LIBWSTREAM_DF_LIB_DIR) -lwstream_df

check-compile:
	$(MAKE) clean
	$(MAKE) all

check-output: test
	../timeout.sh $(TIMEOUT_SECONDS)s ./test > output.log 2>&1
	./check_output.py output.log

clean:
	-rm -f test *.ost *.out *.log
