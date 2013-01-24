#!/bin/sh

if [ -z $OMP_NUM_THREADS ]
then
    OMP_NUM_THREADS=$(grep "physical id" /proc/cpuinfo | wc -l)
fi

if [ -z $CONFIG_NAME ]
then
    echo "Please set CONFIG_NAME." >&2
    exit 1
fi

mkdir -p $CONFIG_NAME
OMP_NUM_THREADS=$OMP_NUM_THREADS time $@ > "$CONFIG_NAME/log" 2>&1

if [ ! -f "wqueue_matrix.out" ]
then
    echo "Could not find wqueue_matrix.out." >&2
    exit 1
fi

mv wqueue_matrix.out $CONFIG_NAME/wqueue_matrix.out

for type in pushes_samel2 pushes_samel3 pushes_remote steals_owncached steals_ownqueue steals_samel2 steals_samel3 steals_remote steals_pushed bytes_l1 bytes_l2 bytes_l3 bytes_rem tasks_executed
do
    rm -f $CONFIG_NAME/$type.out
    for i in $(seq 0 $OMP_NUM_THREADS)
    do
	val=$(grep "Thread $i: $type" $CONFIG_NAME/log | sed 's/^.* = \(.*\)/\1/')
	echo "$i $val" >> $CONFIG_NAME/$type.out
    done
done

NUM_TASKS=$(echo "0" $(cat $CONFIG_NAME/tasks_executed.out | awk '{print $2;}' | sed 's/^/+ /') "0" | bc)

MAX_TASKS=$(cat $CONFIG_NAME/{steals_owncached,steals_ownqueue,steals_samel2,steals_samel3,steals_remote,steals_pushed}.out | \
    awk '{print $2;}' | awk '$0>x{x=$0};END{print x}')

MAX_BYTES=$(cat $CONFIG_NAME/{bytes_l1,bytes_l2,bytes_l3,bytes_rem}.out | \
    awk '{print $2;}' | awk '$0>x{x=$0};END{print x}')

for type in pushes_samel2 pushes_samel3 pushes_remote steals_pushed steals_owncached steals_ownqueue steals_samel2 steals_samel3 steals_remote bytes_l1 bytes_l2 bytes_l3 bytes_rem tasks_executed
do
    TYPE_PREFIX=$(echo -n $type | sed 's/\([^_]*\)_.*/\1/')
    if [ $TYPE_PREFIX = "steals" -o $TYPE_PREFIX = "pushes" -o $TYPE_PREFIX = "tasks" ]
    then
	YMAX="$MAX_TASKS"
	UNIT=""
    elif [ $TYPE_PREFIX = "bytes" ]
    then
	YMAX="$MAX_BYTES"
	UNIT="[bytes]"
    fi

    sed "s|@@OUTFILE_PNG@@|$CONFIG_NAME/$type.png|" "$(dirname $0)/wqueue_singlestat.gnuplot" \
	| sed "s|@@OUTFILE_EPS@@|$CONFIG_NAME/$type.eps|" \
	| sed "s|@@INFILE@@|$CONFIG_NAME/$type.out|" \
	| sed "s|@@XLABEL@@|Worker ID|" \
	| sed "s|@@YLABEL@@|$type $UNIT|" \
	| sed "s|@@XRANGE@@|[0:$OMP_NUM_THREADS]|" \
	| sed "s|@@YRANGE@@|[0:$YMAX.0]|" \
	| gnuplot
done

cat $CONFIG_NAME/log

sed "s|@@OUTFILE_PNG@@|$CONFIG_NAME/heatmap.png|" "$(dirname $0)/wqueue_heatmap.gnuplot" \
    | sed "s|@@OUTFILE_EPS@@|$CONFIG_NAME/heatmap.eps|" \
    | sed "s|@@INFILE_MATRIX@@|$CONFIG_NAME/wqueue_matrix.out|" \
    | gnuplot