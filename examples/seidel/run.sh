#!/bin/bash

echo "Executing Seidel on a matrix of size 2^$1 (tiled by a factor of 2^$2) with $3 iterations."
./seidel -s $1 -b $2 -r $3
./wavefront_stream_seidel -s $1 -b $2 -r $3
./stream_full_expanded_array_seidel -s $1 -b $2 -r $3
./starss_to_stream -s $1 -b $2 -r $3

if [ -f cilk_seidel ]
then
    ./cilk_seidel --nproc 24 -s $1 -b $2 -r $3
fi
if [ -f ompss_seidel ]
then
    OMP_NUM_THREADS=24 ./ompss_seidel -s $1 -b $2 -r $3
fi
