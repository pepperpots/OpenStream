#!/bin/sh

MAX_THREADS=$(grep "physical id" /proc/cpuinfo | wc -l)

if [ -z $OMP_NUM_THREADS ]
then
    OMP_NUM_THREADS=$MAX_THREADS
fi

if [ -z $CONFIG_NAME ]
then
    echo "Please set CONFIG_NAME." >&2
    exit 1
fi

PROFILER_CMD=""
if [ ! -z $USE_IBS ]
then
    PROFILER_CMD="ibs_profile -m $CONFIG_NAME/procmap.map -o $CONFIG_NAME/ibsraw.pfd"

    if [ $OMP_NUM_THREADS -eq $MAX_THREADS ]
    then
	OMP_NUM_THREADS=$(($MAX_THREADS - 1))
    fi
fi

mkdir -p $CONFIG_NAME

echo -n "Run... "
OMP_NUM_THREADS=$OMP_NUM_THREADS time $PROFILER_CMD $@ > "$CONFIG_NAME/log" 2>&1

if [ $? -ne 0 ]
then
    cat $CONFIG_NAME/log
    exit 1
fi

echo "done ["$(grep -e '^[0-9]*\.[0-9]*$' $CONFIG_NAME/log)"]"

if [ ! -f "wqueue_matrix.out" ]
then
    echo "Could not find wqueue_matrix.out." >&2
    exit 1
fi

mv wqueue_matrix.out $CONFIG_NAME/wqueue_matrix.out

for type in pushes_samel2 pushes_samel3 pushes_remote steals_owncached steals_ownqueue steals_samel2 steals_samel3 steals_remote steals_pushed bytes_l1 bytes_l2 bytes_l3 bytes_rem tasks_executed
do
    echo -n "Prepare $type... "
    rm -f $CONFIG_NAME/$type.out
    for i in $(seq 0 $OMP_NUM_THREADS)
    do
	val=$(grep "Thread $i: $type" $CONFIG_NAME/log | sed 's/^.* = \(.*\)/\1/')
	echo "$i $val" >> $CONFIG_NAME/$type.out
    done
    echo "done."
done

if [ ! -z $USE_IBS ]
then
    echo -n "Ibs_op_accum... "
    for CPU in $(seq 0 $(($OMP_NUM_THREADS - 1)))
    do
	echo -n "$CPU "
	ibs_op_accum -i $CONFIG_NAME/ibsraw.pfd -o $CONFIG_NAME/ibs-accum-cpu$CPU.pfd --max --match-cpu $CPU
    done
    echo

    for type in ld_op_cnt der_local_l1_rhits der_local_l1_or_l2 der_local_l3 der_local_dram der_remote_l1_l2_l3 der_remote_dram \
	der_local_latency der_local_l1_or_l2_lat der_local_l3_lat der_local_dram_lat \
	der_remote_latency der_remote_l1_l2_l3_lat der_remote_dram_lat \
	st_op_cnt all_op_cnt
    do
	rm -f $CONFIG_NAME/ibs_$type.out $CONFIG_NAME/ibs_$type.per_load.out
	echo -n "Prepare $type... "

	for CPU in $(seq 0 $(($OMP_NUM_THREADS - 1)))
	do
	    echo $CPU $(pfd_dump $CONFIG_NAME/ibs-accum-cpu$CPU.pfd -d s.$type -r 2>/dev/null) >> $CONFIG_NAME/ibs_$type.out
	done

	if [ $type != "ld_op_cnt" ]
	then
	    for CPU in $(seq 0 $(($OMP_NUM_THREADS - 1)))
	    do
		echo $CPU $(echo "scale=10; " $(pfd_dump $CONFIG_NAME/ibs-accum-cpu$CPU.pfd -d s.$type -r 2>/dev/null) " / " $(pfd_dump $CONFIG_NAME/ibs-accum-cpu$CPU.pfd -d s.ld_op_cnt -r 2>/dev/null) | bc) >> $CONFIG_NAME/ibs_$type.per_load.out
	    done
	fi

	echo "done."
    done
fi

NUM_TASKS=$(echo "0" $(cat $CONFIG_NAME/tasks_executed.out | awk '{print $2;}' | sed 's/^/+ /') "0" | bc)

MAX_TASKS=$(cat $CONFIG_NAME/{steals_owncached,steals_ownqueue,steals_samel2,steals_samel3,steals_remote,steals_pushed}.out | \
    awk '{print $2;}' | awk '$0>x{x=$0};END{print x}')

MAX_BYTES=$(cat $CONFIG_NAME/{bytes_l1,bytes_l2,bytes_l3,bytes_rem}.out | \
    awk '{print $2;}' | awk '$0>x{x=$0};END{print x}')

for type in pushes_samel2 pushes_samel3 pushes_remote \
    steals_pushed steals_owncached steals_ownqueue steals_samel2 steals_samel3 steals_remote \
    bytes_l1 bytes_l2 bytes_l3 bytes_rem tasks_executed \
    ibs_der_local_l1_rhits ibs_der_local_l1_or_l2 ibs_der_local_l3 ibs_der_local_dram ibs_der_remote_l1_l2_l3 ibs_der_remote_dram \
    ibs_der_local_latency ibs_der_local_l1_or_l2_lat ibs_der_local_l3_lat ibs_der_local_dram_lat \
    ibs_der_remote_latency ibs_der_remote_l1_l2_l3_lat ibs_der_remote_dram_lat \
    ibs_ld_op_cnt ibs_st_op_cnt ibs_all_op_cnt \
    ibs_der_local_l1_rhits.per_load ibs_der_local_l1_or_l2.per_load ibs_der_local_l3.per_load ibs_der_local_dram.per_load ibs_der_remote_l1_l2_l3.per_load ibs_der_remote_dram.per_load \
    ibs_der_local_latency.per_load ibs_der_local_l1_or_l2_lat.per_load ibs_der_local_l3_lat.per_load ibs_der_local_dram_lat.per_load \
    ibs_der_remote_latency.per_load ibs_der_remote_l1_l2_l3_lat.per_load ibs_der_remote_dram_lat.per_load \
    ibs_st_op_cnt.per_load ibs_all_op_cnt.per_load
do
    TYPE_PREFIX=$(echo -n $type | sed 's/\([^_]*\)_.*/\1/')
    if [ $TYPE_PREFIX = "steals" -o $TYPE_PREFIX = "pushes" -o $TYPE_PREFIX = "tasks" ]
    then
	YRANGE="set yrange [0:$MAX_TASKS.0]"
	UNIT=""
    elif [ $TYPE_PREFIX = "bytes" ]
    then
	YRANGE="set yrange [0:$MAX_BYTES.0]"
	UNIT="[bytes]"
    else
	YRANGE=""
    fi

    echo -n "$type... "
    sed "s|@@OUTFILE_PNG@@|$CONFIG_NAME/$type.png|" "$(dirname $0)/wqueue_singlestat.gnuplot" \
	| sed "s|@@OUTFILE_EPS@@|$CONFIG_NAME/$type.eps|" \
	| sed "s|@@INFILE@@|$CONFIG_NAME/$type.out|" \
	| sed "s|@@XLABEL@@|Worker ID|" \
	| sed "s|@@YLABEL@@|$type $UNIT|" \
	| sed "s|@@XRANGE@@|[0:$OMP_NUM_THREADS]|" \
	| sed "s|@@YRANGE@@|$YRANGE|" \
	| gnuplot 2> $CONFIG_NAME/gnuplotlog

    if [ $? -ne 0 ]
    then
	cat $CONFIG_NAME/gnuplotlog
	exit 1
    fi
    echo "done."
done

cat $CONFIG_NAME/log

echo -n "Heatmap... "
sed "s|@@OUTFILE_PNG@@|$CONFIG_NAME/heatmap.png|" "$(dirname $0)/wqueue_heatmap.gnuplot" \
    | sed "s|@@OUTFILE_EPS@@|$CONFIG_NAME/heatmap.eps|" \
    | sed "s|@@INFILE_MATRIX@@|$CONFIG_NAME/wqueue_matrix.out|" \
    | gnuplot
echo "done."
