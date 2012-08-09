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
set xlabel '$4'
set ylabel '$5'
set output '$1.$n.$b.$d.$type'
set title '$3 (n=$n; b=$b; d=$d)'
plot '$1.$n.$b.$d.data' title 'native' w $2, \\
     '$1-nofences.$n.$b.$d.data' title 'nofences' w $2, \\
     '$1-c11.$n.$b.$d.data' title 'c11' w $2, \\
     '$1-dumbc11.$n.$b.$d.data' title 'seqcst' w $2
set output '$1.$n.$b.$d.log.$type'
set title '$3 (n=$n; b=$b; d=$d; log)'
set log x
plot '$1.$n.$b.$d.data' title 'native' w $2, \\
     '$1-nofences.$n.$b.$d.data' title 'nofences' w $2, \\
     '$1-c11.$n.$b.$d.data' title 'c11' w $2, \\
     '$1-dumbc11.$n.$b.$d.data' title 'seqcst' w $2
EOF
}

tests='test-cdeque test-cdeque-nofences test-cdeque-c11 test-cdeque-dumbc11'

if [ -n "$(find . -maxdepth 1 -name "test-*.$n.$b.$d.log")" ]; then
	for t in $tests; do
		awk '{print $NF, $1}' $t.$n.$b.$d.log |
		sort -n >time.$t.$n.$b.$d.data
	done
	plot time.test-cdeque points		\
	    'Execution time'				\
	    'effective steals' 'execution time (s)'
	for t in $tests; do
		awk '{print $(NF-2), $1}' $t.$n.$b.$d.log |
		sort -n >time_exp_tput.$t.$n.$b.$d.data
	done
	plot time_exp_tput.test-cdeque points			\
	    'Execution time'						\
	    'expected steal throughput (s⁻¹)' 'execution time (s)'
	for t in $tests; do
		awk '{print $NF/$1, $1}' $t.$n.$b.$d.log |
		sort -n >time_eff_tput.$t.$n.$b.$d.data
	done
	plot time_eff_tput.test-cdeque points			\
	    'Execution time'						\
	    'effective steal throughput (s⁻¹)' 'execution time (s)'
	for t in $tests; do
		awk '{print $NF/$1, 2*$(NF-3)/$1}' $t.$n.$b.$d.log |
		sort -n >tput_eff_tput.$t.$n.$b.$d.data
	done
	plot tput_eff_tput.test-cdeque points	\
	    'Critical path throughput'			\
	    'effective steal throughput (s⁻¹)'		\
	    'push/take throughput (s⁻¹)'
fi
