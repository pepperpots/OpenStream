set terminal png size 600,400
set output "@@OUTFILE_PNG@@"

set tic scale 0
set palette defined (0 1.0 1.0 1.0, 0 0 0 0)
set view map

set title "Workqueue data transfers between workers [bytes]"
set xlabel "Consumer"
set ylabel "Producer"
splot '@@INFILE_MATRIX@@' matrix with image title ""

set terminal postscript color enhanced
set output "@@OUTFILE_EPS@@"
splot '@@INFILE_MATRIX@@' matrix with image title ""