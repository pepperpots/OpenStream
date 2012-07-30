b=3
d=16
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

gnuplot - <<EOF
set terminal $type
set output 'test-cdeque.$n.$b.$d.$type'
plot 'test-cdeque.$n.$b.$d.data' title 'native' w lines, \\
     'test-cdeque-nofences.$n.$b.$d.data' title 'nofences' w lines, \\
     'test-cdeque-c11.$n.$b.$d.data' title 'c11' w lines, \\
     'test-cdeque-dumbc11.$n.$b.$d.data' title 'seqcst' w lines
set output 'test-cdeque.$n.$b.$d.log.$type'
set log x
plot 'test-cdeque.$n.$b.$d.data' title 'native' w lines, \\
     'test-cdeque-nofences.$n.$b.$d.data' title 'nofences' w lines, \\
     'test-cdeque-c11.$n.$b.$d.data' title 'c11' w lines, \\
     'test-cdeque-dumbc11.$n.$b.$d.data' title 'seqcst' w lines
EOF
