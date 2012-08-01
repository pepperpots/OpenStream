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
	awk -v t=$t -v targs="-b $b -d $d -n $nthread" -v ni=$1	\
	-v expected=$2 -v total=$nsteal '
	BEGIN {
		a = 0
		b = 1
		f = expected / total
		if (expected == 0) {
			print 0.0
			exit
		}
		for (i = 0; i < ni; ++i) {
			cmd = t " " targs " -s " expected " -f " f
			cmd | getline
			cmd | getline
			close(cmd)
			thief_time = 0
			for (j = 2; j <= NF - 5; ++j)
				thief_time += $j
			thief_time /= NF - 6
			if (thief_time < $1)
				b = f
			else
				a = f
			f = (a + b) / 2
		}
		print f
	}
'
}

iter()
{
	for i in `seq 1 $1`; do
		$t -b $b -d $d -n $nthread -s $2 -f $3 | tail -n 1
	done
}

run()
{
	log=$t.$nthread.$b.$d.$1.log
	echo tuning $t -b $b -d $d -s $1 >&2
	f=$(tune 10 $1)
	echo $t -b $b -d $d -s $1 -f $f \> $log >&2
	[ $dry ] || iter $niter $1 $f >$log
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
	for t in $tests; do
		echo $t prerun >&2
		[ $dry ] || iter 10 0 >/dev/null
		run 0
		s=1
		n10=$((nsteal / 10))
		while [ $s -lt $n10 ]; do
			if [ $s -lt $nthread ]; then
				echo skipping $t -b $b -d $d -s $s >&2
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
