include Makefile.config

SUBDIRS=cholesky fft-1d fibo fmradio seidel syntax jacobi knapsack matmul strassen sparse_lu

.PHONY: $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean: $(patsubst %,clean-%,$(SUBDIRS))

clean-%:
	$(MAKE) -C $* clean