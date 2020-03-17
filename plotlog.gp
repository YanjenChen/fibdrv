reset
set term png enhanced font 'Verdana,10'
set output 'timelog.png'
set autoscale
set xlabel 'size'
set ylabel 'time(ns)'
set title 'fib runtime'
set key left top

plot [:][:]'fibadd.time.log' using 1:2 with line title 'add',\
'fibfast.time.log' using 1:2 with line title 'fast w/o clz smul',\
'fibfastsmul.time.log' using 1:2 with line title 'fast with smul',\
'fibfastclz.time.log' using 1:2 with line title 'fast with clz',\
