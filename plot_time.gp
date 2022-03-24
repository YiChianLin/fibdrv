set title "user time and kernel time"
set xlabel "n^{th} of F(n)"
set ylabel "time(ns)"
set terminal png font " Times_New_Roman,12 "
set output "plot_time.png"
set xtics 0 ,10 ,100
set key left 

plot \
"out" using 1 with linespoints linewidth 2 title "user space", \
"out" using 2 with linespoints linewidth 2 title "kernel space", \
"out" using 3 with linespoints linewidth 2 title "user to kernel", \
