WSTREAM_CFLAGS=		$(CFLAGS) -g -O2 -ffast-math -D_GNU_SOURCE -I . -fPIC -Wall -Wextra
WSTREAM_LDFLAGS=	$(LDFLAGS) -lpthread

# PAPI_DIR=		/media/disk/dev/wstream_df/install/
# WSTREAM_CFLAGS+=	-I$(PAPI_DIR)/include
# WSTREAM_LDFLAGS+=	-L$(PAPI_DIR)/lib -lpapi

TEST_PROGS=		test-cdeque test-cdeque-nofences test-cdeque-cas test-cdeque-cas-nofences
TEST_LDFLAGS=		-lm -lrt

all: libwstream_df.so
all-test: $(TEST_PROGS)

libwstream_df.so: wstream_df.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -shared $< -o $@ $(WSTREAM_LDFLAGS)

test-cdeque: test-cdeque.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)
test-cdeque-nofences: test-cdeque.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -DNO_FENCES=1 $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)

test-cdeque-cas: test-cdeque-cas.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)
test-cdeque-cas-nofences: test-cdeque-cas.c wstream_df.h cdeque.h cbuffer.h papi-defs.h
	$(CC) $(WSTREAM_CFLAGS) -DNO_FENCES=1 $< -o $@ $(WSTREAM_LDFLAGS) $(TEST_LDFLAGS)

clean:
	rm -f *~ *.o
	rm -f libwstream_df.so
	rm -f $(TEST_PROGS)

install: all
	cp libwstream_df.so ../install/lib64
