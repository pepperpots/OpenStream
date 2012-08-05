printf '%-25s\t%-9s\t%-9s\t%-9s\t%-9s\t%-9s\t%-9s\t%s\n'	\
    t-n-b-d-s							\
    mean stddev maxdev						\
    thief_avg thief_avgerr90					\
    steal_avg steal_avgok
for log in test-cdeque*.log; do
	label=${log%.log}
	label=$(echo $label |
		sed -e 's/^test-cdeque\./native./' -e 's/^test-cdeque-//' |
		tr . -)
	mean=$(awk '{x+=$1;++n} END{print x/n}' $log)
	stddev=$(awk -v m=$mean '{x+=($1-m)^2;++n} END{print sqrt(x/n)}' $log)
	maxdev=$(awk -v m=$mean '{x=($1-m)^2} x>u{u=x} END{print sqrt(u)}' $log)
	thief_avg=$(awk '{for(i=1;i<=NF-5;++i){x+=$i;++n}} END{print x/n}' $log)
	thief_avgerr90=$(awk						\
	    '{for(i=1;i<=NF-5;++i){x+=$i/(0.9*$1);++n}} END{print x/n}'	\
	    $log)
	steal_avg=$(awk '{x+=$(NF-1)==0?0:$NF/$(NF-1);++n} END{print x/n}' $log)
	steal_avgok=$(awk '{x+=$NF==0?0:$(NF-4)/$NF;++n} END{print x/n}' $log)
	printf '%-25s\t%-9g\t%-9g\t%-9g\t%-9g\t%-9g\t%-9g\t%g\n'	\
	    $label							\
	    $mean $stddev $maxdev					\
	    $thief_avg $thief_avgerr90					\
	    $steal_avg $steal_avgok
done | sort -t - -k 1,1 -k 2n -k 3n -k 4n -k 5n
