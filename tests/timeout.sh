#!/bin/sh

TIMEOUT=$1
shift
ARGS=$@

timeout $TIMEOUT $@
RETVAL=$?

if [ $RETVAL -eq 124 ]
then
    echo "Error: Maximum execution time exceeded" >&2
fi

exit $RETVAL
