include Makefile.config

all:
	$(MAKE) -f extras/Makefile.getdeps
	$(MAKE) -C examples

paraver_support:
	$(MAKE) -f extras/Makefile.getdeps osparaver

papi_support:
	$(MAKE) -f extras/Makefile.getdeps papi

check:
	cd tests ; ./check.sh

run: run-A
perf-test: run-A
run-%:
	$(MAKE) -C examples $@

clean-tuning:
	$(MAKE) -C examples clean-tuning

clean:
	rm -rf $(INST_DIR)
	if [ -d $(GCC_BUILD_DIR) ]; then \
		if [ -f $(GCC_BUILD_DIR)/Makefile ]; then \
			$(MAKE) -C $(GCC_BUILD_DIR) clean; \
		fi \
	fi
	$(MAKE) -C libworkstream_df clean
	$(MAKE) -C examples clean

clean-clean:
	rm -rf $(INST_DIR) $(GCC_BUILD_DIR) $(ARCHIVES_DIR) $(CONTRIB_DIR)
	$(MAKE) -C libworkstream_df clean
	$(MAKE) -C examples clean
