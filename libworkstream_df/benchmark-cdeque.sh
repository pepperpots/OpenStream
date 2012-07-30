breadth=3
depth=16
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

iter()
{
	for i in `seq 1 $1`; do
		$t $workload -n $nthread -s $2 2>&1 | head -n 1
	done
}

do_tree()
{
	b=$1
	d=$2
	workload="-b $b -d $d"
	nsteal=$(awk -v b=$b -v d=$d -v n=$nthread '
	BEGIN {
		total = 0
		row = b
		for (i = 0; i < d; ++i) {
			row *= b
			total += row
		}
		print total
	}
	')
	echo jobs $nsteal >&2

	tests='./test-cdeque ./test-cdeque-nofences ./test-cdeque-c11 ./test-cdeque-dumbc11'
	for t in $tests; do
		echo $t prerun >&2
		[ $dry ] || iter 10 0 >/dev/null
		log=$t.$nthread.$b.$d.0.log
		echo $t -b $b -d $d -s 0 \> $log >&2
		[ $dry ] || iter $niter 0 >$log
		s=1
		n10=$((nsteal / 10))
		while [ $s -lt $n10 ]; do
			log=$t.$nthread.$b.$d.$s.log
			if [ $s -lt $nthread ]; then
				echo $t -b $b -d $d -s $s skip >&2
			else
				echo $t -b $b -d $d -s $s \> $log >&2
				[ $dry ] || iter $niter $s >$log
			fi
			s=$((s * 10))
		done
		for sratio10 in 1 2 4 6 8; do
			s=$((sratio10 * nsteal / 10))
			log=$t.$nthread.$b.$d.$s.log
			echo $t -b $b -d $d -s 0.$sratio10 \> $log >&2
			[ $dry ] || iter $niter 0.$sratio10 >$log
		done
		[ $dry ] || for log in $t.$nthread.$b.$d.*.log; do
			s=${log%.log}
			s=${s##*.}
			awk "{x+=\$3;++n} END{print $s, x/n}" $log
		done | sort -n >$t.$nthread.$b.$d.data
	done
}

do_tree $breadth $depth
