include Makefile.config

all:
	$(MAKE) -f extras/Makefile.getdeps
	$(MAKE) -C examples

check:
	echo "To be implemented"

perf-test:
	echo "Comming soon."

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
