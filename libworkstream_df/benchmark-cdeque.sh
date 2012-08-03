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
	awk -v t=$t -v targs="$testargs"		\
	    -v n=$nthread -v ni=$1			\
	    -v hint=$3 -v expected=$2 -v total=$nsteal '
	BEGIN {
		a = hint
		b = 1 / (n - 1)
		f = expected / total / (n - 1)
		if (expected == 0) {
			print 0.0
			exit
		}
		if (f < a)
			a = f
		est = f
		for (i = 0; i < ni; ++i) {
			cmd = t " " targs " -s " expected " -f " f
			cmd | getline
			cmd | getline
			close(cmd)
			thief_time = 0
			for (j = 2; j <= n; ++j)
				thief_time += $j
			thief_time /= n - 1
			printf "%f => %f / %f (%f)\n",
			    f, thief_time, $1, thief_time / $1 | "cat >&2"
			est = f
			if (thief_time < $1)
				b = f
			else
				a = f
			f = (a + b) / 2
		}
		print est
	}
'
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
	echo tuning $t $testargs -s $1 >&2
	f=$(tune 10 $1 $f)
	echo $t $testargs -s $1 -f $f \> $log >&2
	if [ $dry ]; then
		$t $testargs -s $1 -f $f
	else
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
			if [ $s -lt $nthread ]; then
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
