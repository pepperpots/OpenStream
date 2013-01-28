set terminal png size 600,200
set output "@@OUTFILE_PNG@@"

@@XRANGE@@
@@YRANGE@@

set title "@@TITLE@@"
set xlabel "@@XLABEL@@"
set ylabel "@@YLABEL@@"
x0=NaN
y0=NaN
plot '@@INFILE@@' using @@COLUMNSPEC@@ with @@LINETYPE@@ title ""

set terminal postscript color enhanced
set output "@@OUTFILE_EPS@@"
plot '@@INFILE@@' using @@COLUMNSPEC@@ with @@LINETYPE@@ title ""