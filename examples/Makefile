include Makefile.config

SUBDIRS=cholesky fft_1d fibo fmradio seidel syntax jacobi knapsack matmul strassen sparse_lu bzip2 mergesort

.PHONY: $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

.runs: run-A
run-%:
	./tune.sh -c $*
	./run.sh -c $*

clean-tuning:
	find `uname -n`_config -name "*.config" -exec rm {} \;

clean: $(patsubst %,clean-%,$(SUBDIRS))

clean-%:
	$(MAKE) -C $* clean