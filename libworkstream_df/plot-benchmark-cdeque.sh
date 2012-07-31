b=3
d=15
n=2
type=png
while getopts b:d:n:T: opt; do
	case $opt in
	b)
		b=$OPTARG
		;;
	d)
		d=$OPTARG
		;;
	n)
		n=$OPTARG
		;;
	T)
		type=$OPTARG
	esac
done
shift $((OPTIND - 1))

plot()
{
	gnuplot - <<EOF
set terminal $type
set output '$1.$n.$b.$d.$type'
plot '$1.$n.$b.$d.data' title 'native' w lines, \\
     '$1-nofences.$n.$b.$d.data' title 'nofences' w lines, \\
     '$1-c11.$n.$b.$d.data' title 'c11' w lines, \\
     '$1-dumbc11.$n.$b.$d.data' title 'seqcst' w lines
set output '$1.$n.$b.$d.log.$type'
set log x
plot '$1.$n.$b.$d.data' title 'native' w lines, \\
     '$1-nofences.$n.$b.$d.data' title 'nofences' w lines, \\
     '$1-c11.$n.$b.$d.data' title 'c11' w lines, \\
     '$1-dumbc11.$n.$b.$d.data' title 'seqcst' w lines
EOF
}

tests='test-cdeque test-cdeque-nofences test-cdeque-c11 test-cdeque-dumbc11'

for t in $tests; do
	for log in $t.$n.$b.$d.*.log; do
		s=${log%.log}
		s=${s##*.}
		awk "{x+=\$1;++n} END{print $s, x/n}" $log
	done | sort -n >$t.$n.$b.$d.data
done
plot test-cdeque

for t in $tests; do
	for log in $t.$n.$b.$d.*.log; do
		s=${log%.log}
		s=${s##*.}
		if [ $s -eq 0 ]; then
			echo 0 1.0
		else
			awk "{x+=\$(NF-4)/\$NF;++n} END{print $s, x/n}" $log
		fi
	done | sort -n >steal.$t.$n.$b.$d.data
done
plot steal.test-cdeque

for t in $tests; do
	for log in $t.$n.$b.$d.*.log; do
		s=${log%.log}
		s=${s##*.}
		if [ $s -eq 0 ]; then
			echo 0 1.0
		else
			awk "{x+=\$(NF-4)/\$1;++n} END{print $s, x/n}" $log
		fi
	done | sort -n >stealtime.$t.$n.$b.$d.data
done
plot stealtime.test-cdeque
