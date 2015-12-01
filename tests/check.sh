#!/bin/bash

die () {
    echo $1 >&2
    exit 1
}

NUM_OK=0
NUM_FAILED=0
NUM_TESTS=0

KEEP_GOING="false"

[ "x$1" = "x-k" ] && KEEP_GOING="true"

for CHECK in compile output
do
    for TEST in $(find . -maxdepth 1 -type d -name "t*" | sort -n)
    do
	NUM_TESTS=$(($NUM_TESTS + 1))
	TEST_NAME=$(basename $TEST)
	echo -n "$TEST_NAME: Running check '$CHECK'... "

	make -C "$TEST" check-$CHECK > "$TEST/check-$CHECK.log" 2>&1
	RETVAL=$?

	if [ $RETVAL -ne 0 ]
	then
	    NUM_FAILED=$(($NUM_FAILED + 1))
	    echo "Error. Check $TEST/check-$CHECK.log for details."
	    [ $KEEP_GOING = "true" ] || die ""
	else
	    NUM_OK=$(($NUM_OK + 1))
	    echo "done."
	fi
    done
done

echo "$NUM_TESTS tests: $NUM_OK OK, $NUM_FAILED FAILED."

[ $NUM_FAILED -eq 0 ] || exit 1
