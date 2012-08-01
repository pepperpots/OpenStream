TESTS = seq_fftw radix_fft_stream_pure barrier_fft

#INST_DIR = ../../install/
INST_DIR = /media/disk/dev/tstar/open-stream-code/install/
FFTW_INST_DIR = /media/disk/dev/install

INSTALL_DIR = ${INST_DIR}
LIB_DIR = ${INSTALL_DIR}/lib64
BIN_DIR = ${INSTALL_DIR}/bin
GCC = ${BIN_DIR}/gcc
LIBWSTREAM_DF_DIR = $(LIB_DIR)
###########################################################################

CFLAGS =  -Wall -std=c99 -O2  -ffast-math -Wl,-rpath,$(INST_DIR)/lib64 -g  #-w -fdump-tree-all-all #-Werror
LDFLAGS = -L${LIB_DIR} -lm -L/usr/lib/lapack/ -llapack -rdynamic -Wl,-rpath,$(LIB_DIR) -L${FFTW_INST_DIR}/lib/ -lfftw3 -I$(FFTW_INST_DIR)/include -Wl,-rpath,$(FFTW_INST_DIR)/lib # Do not link all test cases with Wstream_Df as it launches worker threads even in seq. comp. for now.

all: $(TESTS)

seq_fftw: seq_fftw.c
	$(GCC) $(CFLAGS) $(LDFLAGS) seq_fftw.c -o seq_fftw -fno-inline

radix_fft_stream_pure: radix_fft_stream_pure.c
	$(GCC) $(CFLAGS) $(LDFLAGS) radix_fft_stream_pure.c -o radix_fft_stream_pure -fopenmp -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df

#.c:
#	$(GCC) $(CFLAGS) $(LDFLAGS) -fopenmp $< -o $* -fdump-tree-all-all

clean:
	rm -f $(TESTS) *~ *.c.* *.s *.raw *.out *.txt $(MEM_CST_TESTS)

run:
	./run.sh 19 2 50




CFLAGS_ += -Wall -std=c99 -O3 -ffast-math
LDFLAGS_ += -L${LIB_DIR} -lm -rdynamic -Wl,-rpath,$(LIB_DIR)

MEM_CST_TESTS = barrier_fft barrier_fft_c11 barrier_fft_dumbc11 barrier_fft_nofences

mem_cst_test: $(MEM_CST_TESTS) seq_pref
seq_pref: barrier_fft.c
	$(GCC) $(CFLAGS_) $^ -o $@ $(LDFLAGS_)

barrier_%: CFLAGS_ += -fopenmp -fdump-tree-ompexp-all
barrier_fft: LDFLAGS_ += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df
barrier_fft_c11: LDFLAGS_ += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df_c11
barrier_fft_dumbc11: LDFLAGS_ += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df_dumbc11
barrier_fft_nofences: LDFLAGS_ += -Wl,-rpath,$(LIBWSTREAM_DF_DIR) -L${LIBWSTREAM_DF_DIR} -lwstream_df_nofences
barrier_%: barrier_fft.c
	$(GCC) $(CFLAGS_) $^ -o $@ $(LDFLAGS_)
