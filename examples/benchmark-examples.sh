base_dir=./
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


niter=30
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
	$t $options | head -n 1
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


test_version=x86

size=35
cutoff=1
dir='fibo'
tests="${test_version}_fibo ${test_version}_fibo_c11 ${test_version}_fibo_dumbc11" # ${test_version}_fibo_nofences'
options=" -n $size -c $cutoff"
discrim="$size.$cutoff"
run

size=10
block=1
iter=20
dir='seidel'
tests="${test_version}_seidel ${test_version}_seidel_c11 ${test_version}_seidel_dumbc11" # ${test_version}_seidel_nofences'
options=" -s $size -b $block -r $iter"
discrim="$size.$block.$iter"
run

dir='fft-1d'
tests="${test_version}_fft ${test_version}_fft_c11 ${test_version}_fft_dumbc11" # ${test_version}_fft_nofences'
options=" "
discrim="fg"
run

dir='knapsack'
tests="${test_version}_knapsack ${test_version}_knapsack_c11 ${test_version}_knapsack_dumbc11" # ${test_version}_knapsack_nofences'
options=" -n2 "
discrim="conf2"
run

dir='matmul'
tests="${test_version}_matmul ${test_version}_matmul_c11 ${test_version}_matmul_dumbc11" # ${test_version}_matmul_nofences'
options=" 256 "
discrim="256x256"
run

dir='strassen'
tests="${test_version}_strassen ${test_version}_strassen_c11 ${test_version}_strassen_dumbc11" # ${test_version}_strassen_nofences'
options=" 512 "
discrim="512x512"
run

dir='jacobi'
tests="${test_version}_jacobi ${test_version}_jacobi_c11 ${test_version}_jacobi_dumbc11" # ${test_version}_jacobi_nofences'
options=" 3 "
discrim="conf3"
run

