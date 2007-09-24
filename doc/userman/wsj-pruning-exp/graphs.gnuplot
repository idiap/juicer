set terminal postscript eps enhanced monochrome

# Global settings
set key right bottom
set xlabel "Real Time Factor (xRT)" font "Helvetica,18"
set ylabel "Word Accuracy (%)" font "Helvetica,18"

# Isolated pruning graphs
set xrange [0:40]
set yrange [60:83]
unset label
set label 1 "-1.0%" at 0.5,81.3
set label 2 "-2.0%" at 0.5,80.3

set output "p.eps"
set title "Main beam pruning effect" font "Helvetica,18"
plot 'results/p.txt' using 3:2 with linespoints ps 2 pt 12 notitle , \
     80.9 lt 3 notitle , 79.9 lt 3 notitle

set output "e.eps"
set title "Phone-end beam pruning effect" font "Helvetica,18"
set label 3 "90.0" at 3.03,73.51 left
set label 4 "160.0" at 8.203,82.3 center
set label 5 "250.0" at 32.094,81.9 left
set label 6 "80.0" at 19.862,66.6 left
set label 7 "110.0" at 31.3,78.72 left
plot 'results/e.txt' using 3:2 with linespoints lt 1 title "No main-beam" , \
     'results/p250e.txt' using 3:2 with linespoints lt 1 title "250.0 main-beam" , \
     80.9 lt 3 notitle , 79.9 lt 3 notitle
unset label 3 ; unset label 4 ; unset label 5 ; unset label 6 ; unset label 7
unset arrow
     
#     'results/p.txt' using 3:2 with lines lt 0 title "Previous main-beam only" , \

set output "m.eps"
set title "Histogram (max. hyps) pruning effect" font "Helvetica,18"
set label 3 "800" at 3.078,68.33 left
set label 4 "2000" at 6.336,78.51 left
set label 5 "6000" at 14,78.51 center
set arrow 1 from 13.8,79 to 12.7,81.5 nohead
set arrow 2 from 14.2,79 to 14,81.4 nohead
set label 6 "20000" at 30,78.51 center
set arrow 3 from 29.8,79 to 26.6,81.5 nohead
set arrow 4 from 30.2,79 to 36.3,81.5 nohead
plot 'results/m.txt' using 3:2 with linespoints lt 1 pt 12 title "No main-beam" , \
     'results/p250m.txt' using 3:2 with linespoints lt 22 title "250.0 main-beam" , \
     80.9 lt 3 notitle , 79.9 lt 3 notitle
unset label 3 ; unset label 4 ; unset label 5 ; unset label 6
unset arrow
     
#     'results/p.txt' using 3:2 with lines lt 0 title "Previous main-beam only" , \

# Combined pruning graphs
set xrange [0:15]
set yrange [72:83]
unset label
set label 1 "-1.0%" at 0.5,81.1
set label 2 "-2.0%" at 0.5,80.1

set title "Combined pruning effect (main-beam = 250)" font "Helvetica,18"
set output "p250em.eps"
plot 'results/p250em.txt' index 0 using 4:3 with linespoints title "Main/End Pruning = 250/220" , \
     'results/p250em.txt' index 2 using 4:3 with linespoints title "Main/End Pruning = 250/180" , \
     'results/p250em.txt' index 3 using 4:3 with linespoints title "Main/End Pruning = 250/160" , \
     'results/p250em.txt' index 4 using 4:3 with linespoints title "Main/End Pruning = 250/140" , \
     'results/p250em.txt' index 5 using 4:3 with linespoints title "Main/End Pruning = 250/120" , \
     80.9 lt 3 notitle , 79.9 lt 3 notitle

#     'results/p250em.txt' index 1 using 4:3 with linespoints notitle , \
     
set title "Combined pruning effect (main-beam = 220)" font "Helvetica,18"
set output "p220em.eps"
plot 'results/p220em.txt' index 0 using 4:3 with linespoints title "Main/End Pruning = 220/220" , \
     'results/p220em.txt' index 2 using 4:3 with linespoints title "Main/End Pruning = 220/180" , \
     'results/p220em.txt' index 3 using 4:3 with linespoints title "Main/End Pruning = 220/160" , \
     'results/p220em.txt' index 4 using 4:3 with linespoints title "Main/End Pruning = 220/140" , \
     'results/p220em.txt' index 5 using 4:3 with linespoints title "Main/End Pruning = 220/120" , \
     80.9 lt 3 notitle , 79.9 lt 3 notitle

#     'results/p220em.txt' index 1 using 4:3 with linespoints title "Main/End Pruning = 220/200" , \

set title "Combined pruning effect (main-beam = 200)" font "Helvetica,18"
set output "p200em.eps"
plot 'results/p200em.txt' index 0 using 4:3 with linespoints title "Main/End Pruning = 200/200" , \
     'results/p200em.txt' index 1 using 4:3 with linespoints title "Main/End Pruning = 200/180" , \
     'results/p200em.txt' index 2 using 4:3 with linespoints title "Main/End Pruning = 200/160" , \
     'results/p200em.txt' index 3 using 4:3 with linespoints title "Main/End Pruning = 200/140" , \
     'results/p200em.txt' index 4 using 4:3 with linespoints title "Main/End Pruning = 200/120" , \
     80.9 lt 3 notitle , 79.9 lt 3 notitle

