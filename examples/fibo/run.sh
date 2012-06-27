#!/bin/bash

echo "Executing Fibonacci for for N = $1 (with a cutoff of $2 for the DF versions)"
/usr/bin/time -f "Sequential: \t \t \t %e seconds" ./fibo -n $1 -c $2
/usr/bin/time -f "Streams:  \t \t \t %e seconds" ./stream_fibo -n $1 -c $2
/usr/bin/time -f "Single stream: \t \t \t %e seconds" ./stream_fibo_single_stream -n $1 -c $2
/usr/bin/time -f "Recursive stream: \t \t \t %e seconds" ./stream_recursive_fibo -n $1 -c $2
if [ -f cilk_fibo ]
then
    /usr/bin/time -f "Cilk:  \t \t \t %e seconds" ./cilk_fibo --nproc 8 $1 $2
fi
