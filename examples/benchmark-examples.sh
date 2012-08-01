base_dir=$PWD
logdir=$base_dir/logs
makedir () {
    DIRNAME=$1
    mkdir -p $DIRNAME
    if [ $? -ne 0 ]; then
	echo "ERROR: could not make directory $DIRNAME"
	exit 1
    fi
}

now () {
    echo `TZ=UTC date +"%Y_%m_%d_%H_%M_%S"`
}
[ -d $logdir ] || makedir $logdir
this_log=$logdir/run_`now`
makedir $this_log
this_dump=$this_log/dump
makedir $this_dump
echo "-------------------------------------------------------------------------" >> $this_log/system_config
uname -a >> $this_log/system_config
echo "-------------------------------------------------------------------------" >> $this_log/system_config
cat /proc/cpuinfo >> $this_log/system_config
echo "-------------------------------------------------------------------------" >> $this_log/system_config
cat /proc/meminfo >> $this_log/system_config


niter=200
while getopts t: opt; do
    case $opt in
	t)
	    niter=$OPTARG
    esac
done
shift $((OPTIND - 1))


iter()
{
    for i in `seq 1 $1`; do
	$t $options
    done
}

run()
{
    for test in $tests; do
	echo $test prerun >&2
	t=$dir/$test
	[ $dry ] || iter 5 >/dev/null
	log=$this_dump/$test.$discrim.log
	echo $test $options \> $log >&2
	[ $dry ] || iter $niter >$log

	echo -n "$test \t\t" >> $this_log/"$dir"_$discrim.data
	# get the 3rd quintile of the sorted results and take the average
	cat $log | sort -n | head -n $(((niter * 3) / 5)) | tail -n $((niter / 5)) | awk "{x+=\$1;++n} END{print x/n}" >> $this_log/"$dir"_$discrim.data
    done

    cat $this_log/"$dir"_$discrim.data
}

size=35
cutoff=1
dir='fibo'
tests='barrier_fibo barrier_fibo_c11 barrier_fibo_dumbc11 barrier_fibo_nofences'
options=" -n $size -c $cutoff"
discrim="$size.$cutoff"
run

size=10
block=1
iter=20
dir='seidel'
tests='barrier_seidel barrier_seidel_c11 barrier_seidel_dumbc11' # barrier_seidel_nofences'
options=" -s $size -b $block -r $iter"
discrim="$size.$block.$iter"
run

dir='fft-1d'
tests='barrier_fft barrier_fft_c11 barrier_fft_dumbc11 barrier_fft_nofences'
options=" "
discrim="fg"
run


