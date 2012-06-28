all: libwstream_df.so

#PAPI_DIR = /media/disk/dev/wstream_df/install/

CFLAGS = -g -O2 -ffast-math -D_GNU_SOURCE -I . -fPIC -Wall -Wextra -lpthread #-L$(PAPI_DIR)/lib -lpapi -I$(PAPI_DIR)/include

libwstream_df.so: wstream_df.c wstream_df.h cdeque.h cbuffer.h
	gcc $(CFLAGS) -shared $< -o $@

clean:
	rm -f *~ *.o libwstream_df.so

install: all
	cp libwstream_df.so ../install/lib64