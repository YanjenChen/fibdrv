reset
set term png enhanced font 'Verdana,10'
set output 'timelog.png'
set autoscale
set xlabel 'size'
set ylabel 'time(ns)'
set title 'runtime'

plot [:][:]'time.log' using 1:2 with line title 'kernel',\
'' using 1:3 with line title 'user',\
'' using 1:4 with line title 'kernel to user',\