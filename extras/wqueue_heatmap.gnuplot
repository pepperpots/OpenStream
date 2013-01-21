set terminal png
set output "wqueue_matrix.png"
set size ratio 0.5
set title "Workqueue data transfers between workers [bytes]"
set xlabel "Consumer"
set ylabel "Producer"

set tic scale 0
set palette defined (0 1.0 1.0 1.0, 0 0 0 0)
set view map

splot 'wqueue_matrix.out' matrix with image title ""