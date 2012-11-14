#!/bin/bash
# This script chooses the optimal value for the -b parameter of the
# seidel benchmarks.

MAXDIFF_SB=6
NUM_RUNS=10

function print_usage_and_die()
{
    echo "Usage: $0 [option] benchmark"
    echo
    echo "Options:"
    echo "  -n <size>                Number of colums of the square matrix"
    echo "  -s <power>               Set the number of colums of the square matrix to 1 << <power>"
    echo "  -r <iter>                Number of iterations"
    echo "  -d <maxdiff>             Maximum difference between the matrix size power and the block power"
    echo "                           Default value is $MAXDIFF_SB"
    echo "  -x <num_executions>      Run benchmark <num_executions> times"
    echo "                           Default value is $NUM_RUNS"
    exit 1
}

# Benchmark arguments
ARG_N=""
ARG_S=""
ARG_R=""

# Read arguments from command line
while getopts n:r:s:d:x:h opt
do
    case "$opt" in
	n) ARG_N="$OPTARG";;
	r) ARG_R="$OPTARG";;
	s) ARG_S="$OPTARG";;
	d) MAXDIFF_SB="$OPTARG";;
	x) NUM_RUNS="$OPTARG";;
	[?]|h) print_usage_and_die;;
    esac
done
shift $(($OPTIND-1))

# Check if benchmark was specified on the command line
if [ $# -ne 1 ]
then
    print_usage_and_die
else
    BENCHMARK=$1
fi

# Check for conflicting or mising arguments
if [ ! -z "$ARG_N" -a ! -z "$ARG_S" ]
then
    echo "Please specify either -n or -s" >&2
    exit 1
elif [ -z "$ARG_N" -a -z "$ARG_S" ]
then
    echo "Please specify at least one of the options -n or -s" >&2
    exit 1
fi

# Check if benchmark executable is available
if [ ! -f "$BENCHMARK" ]
then
    echo "Cannot find benchmark \"$BENCHMARK\". Did you forget to build it?" >&2
    exit 1
fi

# Collect benchmark arguments
BENCH_ARGS=""
if [ ! -z "$ARG_R" ]
then
    BENCH_ARGS="$BENCH_ARGS -r$ARG_R "
fi

if [ ! -z "$ARG_S" ]
then
    BENCH_ARGS="$BENCH_ARGS -s$ARG_S "
    LOGSIZE="$ARG_S"
elif [ ! -z "$ARG_N" ]
then
    BENCH_ARGS="$BENCH_ARGS -n$ARG_N "
    LOGSIZE=$(echo "l($ARG_N)/l(2)" | bc -l | cut -f1 -d".")
fi

# Determine which block size powers have to be tested
if [ $LOGSIZE -gt $MAXDIFF_SB ]
then
    BLOCKSIZES=$(seq $(($LOGSIZE - $MAXDIFF_SB)) $LOGSIZE | xargs)
else
    BLOCKSIZES=$(seq 1 $LOGSIZE | xargs)
fi

# Minimal execution time in microseconds
# (avoids floating point handling in the loop)
MIN_AVGTIME_US=""

# Minimal execution time in seconds
MIN_AVGTIME_S=""

echo "Trying block size powers $BLOCKSIZES, $NUM_RUNS executions..."

for BLOCKSIZE in $BLOCKSIZES
do
    echo -n "* Trying -b$BLOCKSIZE"

    BC_ARGS=""
    for i in $(seq $NUM_RUNS)
    do
	echo -n "."

	# Execute the benchmark
	TM=$(./$BENCHMARK $BENCH_ARGS -b$BLOCKSIZE)

	# Check if run was ok
	if [ $? -ne 0 ]
	then
	    echo >&2
	    echo "Oops! Failed to execute ./$BENCHMARK $BENCH_ARGS -b$BLOCKSIZE. That should not have happened!" >&2
	    cat log >&2
	    echo >&2
	    exit 1
	fi

	# Build bc command (time of run 1 + time of run 2 + ...)
	BC_ARGS="$BC_ARGS$TM+"
    done
    echo -n " "

    BC_ARGS="($BC_ARGS 0)/$NUM_RUNS"

    # Calculate average run time in seconds
    AVG=$(echo "scale=6;$BC_ARGS" | bc)

    # Convert to microseconds
    AVG_US=$(echo "scale=6;$AVG*1000000" | bc | cut -f1 -d".")
    echo "average: $AVG seconds"

    # Update minimal execution time if necessary
    if [ -z "$MIN_AVGTIME_US" ] || [ $AVG_US -lt $MIN_AVGTIME_US ]
    then
	MIN_AVGTIME_US=$AVG_US
	MIN_AVGTIME_S=$AVG
	MIN_BLOCKSIZE=$BLOCKSIZE
    fi
done

echo "Best average value for $BENCH_ARGS is $MIN_AVGTIME_S, obtained by using a block size of 2^$MIN_BLOCKSIZE."
echo "Recommended command line: ./$BENCHMARK $BENCH_ARGS -b$MIN_BLOCKSIZE"
