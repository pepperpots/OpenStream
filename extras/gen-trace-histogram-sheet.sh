#!/bin/sh

HISTOGRAM_FILE="task_histogram.gpdata"
NUM_GRAPHS=`grep '#' $HISTOGRAM_FILE | wc -l`

if [ -z "$1" -o "$1" = "--keep" ]
then
    KEEP="true"
else
    KEEP="false"
fi

XLABEL=`awk "NR==1{print;exit}" $HISTOGRAM_FILE | sed 's/#[0-9 ]*: \(.*\)/\1/'`

for i in `seq 2 $NUM_GRAPHS`
do
    TITLE=`awk "NR==$i{print;exit}" $HISTOGRAM_FILE | sed 's/#[0-9 ]*: \(.*\)/\1/'`
    ( echo "set terminal postscript color";
    echo "set title '$TITLE'" ;
    echo "set xlabel '$XLABEL'" ;
    echo "set ylabel ''" ;
    echo "set xrange [0:*]" ;
    echo "set boxwidth 1" ;
    echo "set output \"$i.ps\"" ;
    echo "plot \"$HISTOGRAM_FILE\" using 1:$i with boxes title ''") | gnuplot
    ps2pdf $i.ps
    rm $i.ps
done

pdfjoin --outfile histograms.pdf `seq 2 $NUM_GRAPHS | sed 's/$/.pdf/' | xargs`

if [ $KEEP = "false" ]
then
    rm `seq 2 $NUM_GRAPHS | sed 's/$/.pdf/' | xargs`
fi
