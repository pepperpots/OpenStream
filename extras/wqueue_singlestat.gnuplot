set terminal png size 600,200
set output "@@OUTFILE_PNG@@"

set xrange @@XRANGE@@
set yrange @@YRANGE@@

set boxwidth 1
set style data histograms
set style fill solid 1.0 border -1

set title ""
set xlabel "@@XLABEL@@"
set ylabel "@@YLABEL@@"
plot '@@INFILE@@' using 2 title ""

set terminal postscript color enhanced
set output "@@OUTFILE_EPS@@"
plot '@@INFILE@@' using 2 title ""