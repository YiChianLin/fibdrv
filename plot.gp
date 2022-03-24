set title "Method Compare"
set xlabel "n^{th} of F(n)"
set ylabel "time(ns)"
set terminal png font " Times_New_Roman,12 "
set output "plot.png"
set xtics 0 ,10 ,100
set key left 

plot \
"plot_input" using 1 with linespoints linewidth 2 title "iterative", \
"plot_input" using 2 with linespoints linewidth 2 title "fast doubling", \
"plot_input" using 3 with linespoints linewidth 2 title "fast doubling-ctz", \
