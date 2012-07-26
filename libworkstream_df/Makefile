WSTREAM_CFLAGS=		$(CFLAGS) -g -O2 -ffast-math -D_GNU_SOURCE -I . -fPIC -Wall -Wextra
WSTREAM_LDFLAGS=	$(LDFLAGS) -lpthread

# PAPI_DIR=		/media/disk/dev/wstream_df/install/
# WSTREAM_CFLAGS+=	-I$(PAPI_DIR)/include
# WSTREAM_LDFLAGS+=	-L$(PAPI_DIR)/lib -lpapi

TEST_PROGS=		test-cdeque test-cdeque-c11 test-cdeque-dumbc11
TEST_PROGS+=		test-cdeque-nollscopt test-cdeque-nolwcas test-cdeque-nofences
TEST_PROGS+=		test-cdeque-cas test-cdeque-cas-c11 test-cdeque-cas-dumbc11
TEST_PROGS+=		test-cdeque-cas-nollscopt test-cdeque-cas-nolwcas test-cdeque-cas-nofences
TEST_LDFLAGS=		-lm -lrt

all: libwstream_df.so
all-test: $(TEST_PROGS)

libwstream_df.so: wstream_df.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -shared $< -o $@ $(WSTREAM_LDFLAGS)

test-cdeque: test-cdeque.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)
test-cdeque-c11: test-cdeque.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -DUSE_STDATOMIC=1 $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)
test-cdeque-dumbc11: test-cdeque.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1 $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)
test-cdeque-nollscopt: test-cdeque.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -DNO_LLSC_OPTIMIZATION=1 $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)
test-cdeque-nolwcas: test-cdeque.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -DNO_LLSC_OPTIMIZATION=1 -DNO_LIGHTWEIGHT_CAS=1 $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)
test-cdeque-nofences: test-cdeque.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -DNO_FENCES=1 $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)

test-cdeque-cas: test-cdeque-cas.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)
test-cdeque-cas-c11: test-cdeque-cas.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -DUSE_STDATOMIC=1 $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)
test-cdeque-cas-dumbc11: test-cdeque-cas.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -DUSE_STDATOMIC=1 -DUSE_SEQ_CST_STDATOMIC=1 $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)
test-cdeque-cas-nollscopt: test-cdeque-cas.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -DNO_LLSC_OPTIMIZATION=1 -DNO_LIGHTWEIGHT_CAS=1 $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)
test-cdeque-cas-nolwcas: test-cdeque-cas.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -DNO_LIGHTWEIGHT_CAS=1 $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)
test-cdeque-cas-nofences: test-cdeque-cas.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -DNO_FENCES=1 $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)

clean:
	rm -f *~ *.o
	rm -f libwstream_df.so
	rm -f $(TEST_PROGS)

install: all
	mkdir -p ../install/lib64
	cp libwstream_df.so ../install/lib64
