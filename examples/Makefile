DIRS = fibo seidel fft-1d

build_mem_cst_test:
	for d in $(DIRS); do (cd $$d && make clean mem_cst_test); done

run_mem_cst_test:
	sh ./benchmark-examples.sh -t100