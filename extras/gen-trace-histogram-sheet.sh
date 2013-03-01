#!/bin/sh

HISTOGRAM_FILE="task_histogram.gpdata"
NUM_GRAPHS=`grep '#' $HISTOGRAM_FILE | wc -l`

KEEP="false"
MAX_X="*"

check_next_arg()
{
    if [ $2 -lt 2 ]
    then
	echo "Error: option $1 requires an argument" >&2
	exit 1
    fi
}

check_option_empty()
{
    if [ -z "$2" ]
    then
	echo "Error: option $1 requires an argument" >&2
	exit 1
    fi
}

while [ $# -gt 0 ]
do
    case $1 in
	--keep|-k)
	    KEEP="true"
	    ;;
	--max-x=*)
	    OPTION=`echo $1 | cut -d= -f1`
	    MAX_X=`echo $1 | cut -d= -f2`
	    check_option_empty "$OPTION" "$MAX_X"
	    ;;
	--infile=*)
	    OPTION=`echo $1 | cut -d= -f1`
	    HISTOGRAM_FILE=`echo $1 | cut -d= -f2`
	    check_option_empty "$OPTION" "$HISTOGRAM_FILE"
	    ;;
	*)
	    echo "Unknown option $1."
	    exit 1
	    ;;
    esac
    shift
done

XLABEL=`awk "NR==1{print;exit}" $HISTOGRAM_FILE | sed 's/#[0-9 ]*: \(.*\)/\1/'`

for i in `seq 2 $NUM_GRAPHS`
do
    TITLE=`awk "NR==$i{print;exit}" $HISTOGRAM_FILE | sed 's/#[0-9 ]*: \(.*\)/\1/'`
    ( echo "set terminal postscript color";
    echo "set title '$TITLE'" ;
    echo "set xlabel '$XLABEL'" ;
    echo "set ylabel ''" ;
    echo "set xrange [0:$MAX_X]" ;
    echo "set boxwidth 1" ;
    echo "set output \"$i.ps\"" ;
    echo "plot \"$HISTOGRAM_FILE\" using 1:$i with boxes title ''") | gnuplot
    ps2pdf $i.ps
    rm $i.ps
done

pdfjoin --outfile histograms.pdf `seq 2 $NUM_GRAPHS | sed 's/$/.pdf/' | xargs`

if [ $KEEP != "true" ]
then
    rm `seq 2 $NUM_GRAPHS | sed 's/$/.pdf/' | xargs`
fi
