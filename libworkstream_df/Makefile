WSTREAM_CFLAGS=		$(CFLAGS) -g -O2 -ffast-math -D_GNU_SOURCE -I . -fPIC -Wall -Wextra
WSTREAM_LDFLAGS=	$(LDFLAGS) -lpthread

# PAPI_DIR=		/media/disk/dev/wstream_df/install/
# WSTREAM_CFLAGS+=	-I$(PAPI_DIR)/include
# WSTREAM_LDFLAGS+=	-L$(PAPI_DIR)/lib -lpapi

TEST_DESTDIR=		tests

TEST_KINDS=		-c11 -dumbc11 -llscopt -nolwcas -nofences
TEST_EXP=		$(TEST_DESTDIR)/__TEST__
TEST_EXP+=		$(foreach K,$(TEST_KINDS),$(TEST_DESTDIR)/__TEST__$(K))
TEST_PROGS.cdeque=		$(subst __TEST__,test-cdeque,$(TEST_EXP))
TEST_PROGS.cas=		$(subst __TEST__,test-cas-cdeque,$(TEST_EXP))
TEST_PROGS.straight=	$(subst __TEST__,test-straight-cdeque,$(TEST_EXP))
TEST_PROGS=		$(TEST_PROGS.cdeque) $(TEST_PROGS.straight) $(TEST_PROGS.cas)

TEST_LDFLAGS=		-lm -lrt

all: libwstream_df.so
all-test: $(TEST_PROGS)

libwstream_df.so: wstream_df.c error.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -shared $(filter %.c,$^) -o $@ $(WSTREAM_LDFLAGS)

$(TEST_PROGS.cdeque): test-cdeque.c
$(TEST_PROGS.cas): test-cas-cdeque.c
$(TEST_PROGS.straight): test-straight-cdeque.c cdeque.c
$(TEST_PROGS.straight): WSTREAM_CFLAGS+=-DNO_INLINE_CDEQUE=1

$(TEST_DESTDIR)/test-%c11: WSTREAM_CFLAGS+=-DUSE_STDATOMIC=1
$(TEST_DESTDIR)/test-%-dumbc11: WSTREAM_CFLAGS+=-DUSE_SEQ_CST_STDATOMIC=1
$(TEST_DESTDIR)/test-%-llscopt: WSTREAM_CFLAGS+=-DLLSC_OPTIMIZATION=1
$(TEST_DESTDIR)/test-%-nolwcas: WSTREAM_CFLAGS+=-DNO_LIGHTWEIGHT_CAS=1
$(TEST_DESTDIR)/test-%-nofences: WSTREAM_CFLAGS+=-DNO_FENCES=1

$(TEST_DESTDIR)/test-%: error.c wstream_df.h cdeque.h cbuffer.h papi-defs.h time-util.h
	mkdir -p $(TEST_DESTDIR)
	$(CC) $(WSTREAM_CFLAGS) $(filter %.c,$^) -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)

clean:
	rm -f *~ *.o
	rm -f libwstream_df.so
	rm -f $(TEST_PROGS)

install: all
	mkdir -p ../install/lib64
	cp libwstream_df.so ../install/lib64

.PHONY: all all-test clean install
