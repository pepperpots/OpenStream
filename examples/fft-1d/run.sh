#!/bin/bash

echo "Executing 1-D FFT on a vector of size 2^$1 with $3 iterations."
echo " "
echo -n "Sequential (best FFTW): "
./seq_fftw -s $1 -x $2 -r $3

echo -n "Dataflow radix-FFT (FFTW sub-kernels): "
 ./radix_fft_stream_pure -s $1 -x $2 -r $3

