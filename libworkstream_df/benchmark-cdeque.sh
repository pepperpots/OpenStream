breadth=3
depth=15
nthread=2
niter=40
while getopts b:d:n:ps:t: opt; do
	case $opt in
	b)
		breadth=$OPTARG
		;;
	d)
		depth=$OPTARG
		;;
	n)
		nthread=$OPTARG
		;;
	p)
		dry=1
		;;
	t)
		niter=$OPTARG
	esac
done
shift $((OPTIND - 1))

tune()
{
	echo tuning $t $testargs -s $2 >&2
	awk -f tune-test-cdeque.awk $1 "$t $testargs" $2 $nsteal $nthread
}

iter()
{
	for i in `seq 1 $1`; do
		if [ $3 ]; then
			farg="-f $3"
		else
			farg=
		fi
		$t $testargs -s $2 $farg | tail -n 1
	done
}

run()
{
	log=$t.$nthread.$b.$d.$1.log
	f=$(tune 15 $1)
	if [ $dry ]; then
		echo $t $testargs -s $1 -f $f
		$t $testargs -s $1 -f $f
	else
		echo $t $testargs -s $1 -f $f \> $log >&2
		iter $niter $1 $f >$log
	fi
}

do_tree()
{
	b=$1
	d=$2
	nsteal=$(awk -v b=$b -v d=$d '
	BEGIN {
		total = 0
		if (b == 1)
			total = d
		else {
			row = b
			for (i = 0; i < d; ++i) {
				row *= b
				total += row
			}
		}
		print total
	}
	')
	echo jobs $nsteal >&2

	tests='./test-cdeque ./test-cdeque-nofences ./test-cdeque-c11 ./test-cdeque-dumbc11'
	testargs="-b $b -d $d -n $nthread"

	for t in $tests; do
		echo $t prerun >&2
		[ $dry ] || iter 10 0 >/dev/null
		f=0
		run 0
		s=1
		n10=$((nsteal / 10))
		while [ $s -lt $n10 ]; do
			if [ $s -lt $((nthread * 10)) ]; then
				echo skipping $t $testargs -s $s >&2
			else
				run $s
			fi
			s=$((s * 10))
		done
		for sratio10 in 1 2 4 6 8; do
			s=$((sratio10 * nsteal / 10))
			run $s
		done
	done
}

do_tree $breadth $depth
