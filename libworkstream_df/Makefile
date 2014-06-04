# determine absolute path of *this* Makefile
THIS_DIR = $(dir $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))

# Start editing below

# OpenStream toplevel installation directory
INST_DIR = $(THIS_DIR)/../install

# GCC with C11 support
GCC = $(INST_DIR)/bin/gcc

WSTREAM_CFLAGS=		-g -O2 -ffast-math -D_GNU_SOURCE -I . -fPIC -Wall -Wextra $(CFLAGS) -std=c11 -Wno-unused-parameter
WSTREAM_LDFLAGS=	-lpthread $(LDFLAGS) -lnuma -lrt
WSTREAM_DEPS=		wstream_df.h cdeque.h cbuffer.h alloc.h arch.h config.h trace.h fibers.h list.h profiling.h work_distribution.h
WSTREAM_DEPS+=		cdeque-native.c.h cdeque-c11.c.h cdeque-native.h cdeque-c11.h tsc.h

TEST_DESTDIR=		tests

TEST_KINDS=		-c11 -dumbc11 -llscopt -nolwcas -nofences -nosync
TEST_EXP=		$(TEST_DESTDIR)/__TEST__
TEST_EXP+=		$(foreach K,$(TEST_KINDS),$(TEST_DESTDIR)/__TEST__$(K))
TEST_PROGS.cdeque=		$(subst __TEST__,test-cdeque,$(TEST_EXP))
TEST_PROGS.cas=		$(subst __TEST__,test-cas-cdeque,$(TEST_EXP))
TEST_PROGS.straight=	$(subst __TEST__,test-straight-cdeque,$(TEST_EXP))
TEST_PROGS=		$(TEST_PROGS.cdeque) $(TEST_PROGS.straight) $(TEST_PROGS.cas)

TEST_LDFLAGS=		-lm -lrt
TEST_DEPS=		time-util.h

all: libwstream_df.so
all-test: $(TEST_PROGS)

ifeq ($(PAPI_ENABLED),yes)
WSTREAM_CFLAGS+=       -I$(INST_DIR)/include
WSTREAM_LDFLAGS+=      -L$(INST_DIR)/lib -lpapi
endif

../extras/ostv/configure.ac:
	cd .. ; git submodule init
	cd .. ; git submodule update

convert.h convert.c trace_file.c trace_file.h ansi_extras.h: ../extras/ostv/configure.ac

# Check if library is built using OpenStream's GCC
# If this is the case, add dependency to the gcc binary
GCC_INSTALL_PATH=$(realpath $(GCC))
GENERATED_GCC_PATH=$(realpath ../install/gcc)
ifeq ($(GCC_INSTALL_PATH),$(GENERATED_GCC_PATH))
	GCCDEP=../install/bin/gcc
else
	GCCDEP=
endif

../install/bin/gcc:
	cd .. ; $(MAKE)

libwstream_df.so: wstream_df.c error.c trace.c fibers.c profiling.c work_distribution.c trace_file.c trace_file.h ansi_extras.h convert.c convert.h tsc.c interleave.c numa.c $(WSTREAM_DEPS) $(GCCDEP)
ifeq ($(PAPI_CHECKED),)
	@$(MAKE) -C ../extras -f Makefile.papi_enabled
	@PAPI_ENABLED=`../extras/papi_enabled` ; \
	if [ $$PAPI_ENABLED = "yes" ] ; \
	then \
		if [ ! -f $(INST_DIR)/bin/papi_avail ] ; \
		then \
			echo "PAPI enabled in configuration, but not installed." ;\
			cd ../ ; $(MAKE) papi_support ; cd - ; \
		fi ; \
	fi ;\
	$(MAKE) PAPI_CHECKED=1 PAPI_ENABLED=$$PAPI_ENABLED $@ ;
else
	$(GCC) $(WSTREAM_CFLAGS) -shared $(filter %.c,$^) -o $@ $(WSTREAM_LDFLAGS)
endif

$(TEST_PROGS.cdeque): test-cdeque.c
$(TEST_PROGS.cas): test-cas-cdeque.c
$(TEST_PROGS.straight): test-straight-cdeque.c cdeque.c
$(TEST_PROGS.straight): WSTREAM_CFLAGS+=-DNO_INLINE_CDEQUE=1

$(TEST_DESTDIR)/test-%c11: WSTREAM_CFLAGS+=-DUSE_STDATOMIC=1
$(TEST_DESTDIR)/test-%-dumbc11: WSTREAM_CFLAGS+=-DUSE_SEQ_CST_STDATOMIC=1
$(TEST_DESTDIR)/test-%-llscopt: WSTREAM_CFLAGS+=-DLLSC_OPTIMIZATION=1
$(TEST_DESTDIR)/test-%-nolwcas: WSTREAM_CFLAGS+=-DNO_LIGHTWEIGHT_CAS=1
$(TEST_DESTDIR)/test-%-nofences: WSTREAM_CFLAGS+=-DNO_FENCES=1
$(TEST_DESTDIR)/test-%-nosync: WSTREAM_CFLAGS+=-DNO_FENCES=1 -DNO_SYNC=1

$(TEST_DESTDIR)/test-%: error.c $(WSTREAM_DEPS) $(TEST_DEPS)
	mkdir -p $(TEST_DESTDIR)
	$(CC) $(WSTREAM_CFLAGS) $(filter %.c,$^) -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)

clean:
	rm -f *~ *.o
	rm -f libwstream_df*.so
	rm -f $(TEST_PROGS)
	rm -f TAGS

install: all
	mkdir -p ../install/lib64
	cp libwstream_df.so ../install/lib64

.PHONY: all all-test clean install


alternative: libwstream_df_c11.so libwstream_df_dumbc11.so libwstream_df_nofences.so
libwstream_df_c11.so: WSTREAM_CFLAGS+=-DUSE_STDATOMIC=1
libwstream_df_dumbc11.so: WSTREAM_CFLAGS+=-DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1
libwstream_df_nofences.so: WSTREAM_CFLAGS+=-DNO_FENCES=1
libwstream_df_%.so: wstream_df.c error.c $(WSTREAM_DEPS)
	$(GCC) $(WSTREAM_CFLAGS) -shared $(filter %.c,$^) -o $@ $(WSTREAM_LDFLAGS)
install-alternative: alternative
	mkdir -p ../install/lib64
	cp libwstream_df_*.so ../install/lib64

TAGS:
	find . -iname "*.[ch]" -print | etags -
