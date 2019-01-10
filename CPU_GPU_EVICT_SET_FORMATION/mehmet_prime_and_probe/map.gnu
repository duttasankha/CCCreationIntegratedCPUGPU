set terminal pdf fontscale 0.6 size 320in, 8in
set border 2 front linetype -1 linewidth 0.5
#unset border
unset key
unset colorbox
#set palette grey negative
#set palette rgb 21,22,23 negative
set palette defined ( \
0	0	0	0 	,\
0.1	1  	0.1	0.1 	,\
0.2	1	0.9	0.7	,\
0.4	0.7	0.4	1	,\
0.6	0.4	0.4	1	,\
0.7	0.4	0.9	1 	,\
0.8	0.8  	0.9	0.8 	,\
1	1	1	1 	) negative
set cbrange [0:1]
set xlabel "Time"
set ylabel "Cache Set"
set y2tics 1 format '' scale 0, 0.001
set x2tics 1 format '' scale 0, 0.001
set my2tics 2
set mx2tics 2
set ytics 4 out nomirror
set xtics 8 out nomirror
set grid front mx2tics my2tics lw 1.5 lt -1 lc rgb '#DDDDDD'
set xrange[-0.5:*]
set x2range[-0.5:*]
set yrange[-0.5:63.5]
set y2range[-0.5:63.5]
set pm3d map
set output prefix.'out.pdf'

load prefix.'victim_default'
plot prefix.'out' matrix with image
plot 'ins_out_nr' matrix with image
load prefix.'victim_default_unset'

set output prefix.'map.pdf'
load prefix.'victim_map'
plot 'out' matrix with image
plot 'ins_out' matrix with image
load prefix.'victim_default_unset'

set output prefix.'map_nr.pdf'
load prefix.'victim_sync'
plot 'out_nr' matrix with image
load prefix.'victim_default_unset'

exit

#Icache figure for paper
set terminal pdf fontscale 0.3 size 3.5in, 0.9in
set lmargin at screen 0.06;
set rmargin at screen 0.995;
set bmargin at screen 0.1;
set tmargin at screen 0.95;
set output prefix.'mini_map_nr.pdf'
set yrange[-0.5:15.5]
set y2range[-0.5:15.5]
set xrange[340.5:620.5]
set x2range[340.5:620.5]
set format x ''
set ylabel "Cache Set" offset 1.4,0
set xlabel ""
set grid front mx2tics my2tics lw 0.5 lt -1 lc rgb '#DDDDDD'
set palette grey negative
set cbrange [0:0.2]
load prefix.'victim_default_unset'
set obj circle at 360,10   size 18  front fill empty border rgb "blue"
set obj circle at 380,5   size 18  front fill empty border rgb "blue"
set obj circle at 412,10   size 18  front fill empty border rgb "blue"
set obj circle at 430,5   size 18  front fill empty border rgb "blue"
set obj circle at 468,10   size 18  front fill empty border rgb "blue"
set obj circle at 495,5   size 18  front fill empty border rgb "blue"
set obj circle at 520,10   size 16  front fill empty border rgb "blue"
set obj circle at 536,5   size 18  front fill empty border rgb "blue"
set obj circle at 570,10   size 20  front fill empty border rgb "blue"
set obj circle at 600,14   size 20  front fill empty border rgb "blue"
plot 'paper_ins_out_nr' matrix with image



set output prefix.'bin.pdf'
plot prefix.'bin' with rgbimage

set xrange[-0.5:*]
set x2range[-0.5:*]
#set terminal pdf fontscale 0.6 size 60in, 8in

set output prefix.'out2.pdf'
plot prefix.'out2' matrix with image

set output prefix.'bin2.pdf'
plot prefix.'bin2' with rgbimage

##
set output prefix.'cleaned.pdf'
load prefix.'victim_cleaned'
plot prefix.'attacker_cleaned' matrix with image
unset ylabel
set ytics 8 format '' out nomirror
load prefix.'victim_unset'


#the rest is removed for using the output of a single run
exit

##FIGURE FOR THE PAPER 

set terminal pdf font 'Arial,8' size 7in, 2.8in
set output prefix.'p_attacker.pdf'
set multiplot layout 1,4
set tmargin at screen 0.94
LX=0.26
LW=0.48
set object 1000 rectangle from screen LX,0.96 to screen LX+LW,1 front fillstyle border 0
set object 1001 rectangle from screen LX+0.005,0.972 to screen LX+0.01, 0.988 front fillstyle noborder fillcolor rgb 'black'
set label  1002 'access measured by attacker' at screen LX+0.015,0.98 font ",7" front
set object 1003 rectangle from screen LX+0.16,0.972 to screen LX+0.165, 0.988 front fillstyle noborder fillcolor rgb '#DDDDDD'
set label  1004 'noise' at screen LX+0.17,0.98 font ",7" front
set object 1005 rectangle from screen LX+0.21,0.972 to screen LX+0.215, 0.988 front fillstyle noborder fillcolor rgb '#478A45'
set label  1006 'true positive' at screen LX+0.22,0.98 font ",7" front
set object 1007 rectangle from screen LX+0.29,0.972 to screen LX+0.295, 0.988 front fillstyle noborder fillcolor rgb '#FF1A1A'
set label  1008 'false positive' at screen LX+0.30,0.98 font ",7" front
set object 1009 rectangle from screen LX+0.38,0.972 to screen LX+0.39, 0.988 front fillstyle transparent solid 0.1 border 0 lw 0.5 fillcolor rgb 'red'
set label  1010 'critical accesses' at screen LX+0.395,0.98 font ",7" front
set arrow  1011 from screen 0.5,0 to screen 0.5,0.94 nohead lc rgb 'black'
# to screen 0.41, 0.985 front fillstyle noborder fillcolor rgb 'black'

set yrange[-0.5:63.5]
set y2range[-0.5:63.5]
set ylabel "Cache Set" offset screen 0.005,0
set xrange[0.5:80.5]
set x2range[0.5:80.5]
set ytics 8 out nomirror
set xtics 8 format '' out nomirror
set grid front mx2tics my2tics lw 0.4 lt -1 lc rgb '#DDDDDD'
set palette defined ( \
0   0   0   0  ,\
0.8 0   0   0  ,\
1   1   1   1   ) negative

set rmargin at screen 0.335
set size 0.3,1

load prefix.'victim_cleaned'

set xlabel "(a) Initial results for Flush+Reload attack\n" offset screen 0,0.005
plot prefix.'attacker_cleaned' matrix with image
unset ylabel
set ytics 8 format '' out nomirror
load prefix.'victim_unset'

## PRIME+PROBE
load '../prime-probe/'.prefix.'victim_cleaned'
set origin 0.5,0
set lmargin at screen 0.51
set rmargin at screen 0.835
set object 2000 circle at 45,58 size 10 front fillstyle transparent solid 0 border lc rgb 'blue' lw 0.4
set xlabel "(c) Initial results for Prime+Probe attack\n" offset screen 0,0.005
plot '../prime-probe/'.prefix.'attacker_cleaned' matrix with image
load '../prime-probe/'.prefix.'victim_unset'
unset object 2000
##

load prefix.'_victim_cleaned2'
set size 0.2,1
set origin 0.3,0
set lmargin at screen 0.345
set rmargin at screen 0.49
set xrange[0.5:36.5]
set x2range[0.5:36.5]
set palette defined ( \
0	1	0.1	0.1 	,\
0.1	0.2 	0.6	0.3 	,\
0.2	0.85	0.85	0.85	,\
0.4	0.6	0.6	0.6	,\
0.6	0.4	0.4	1	,\
0.7	0.4	0.9	1 	,\
0.8	0.9  	0.9	0.9 	,\
1	1	1	1 	) negative
set xlabel "(b) Flush+Reload attack results \nafter noise removal" offset screen 0,0.005
plot prefix.'_out2' matrix with image
load prefix.'_victim_unset2'

##PRIME+PROBE
load '../prime-probe/'.prefix.'_victim_cleaned2'
set origin 0.8,0
set lmargin at screen 0.845
set rmargin at screen 1
set xlabel "(d) Prime+Probe attack results \nafter noise removal" offset screen 0,0.005
set object 2000 circle at 22,14 size 10 front fillstyle transparent solid 0 border lc rgb 'blue' lw 0.4
plot '../prime-probe/'.prefix.'_out2' matrix with image
load '../prime-probe/'.prefix.'_victim_unset2'
unset object 2000
##


##MAGNIFY

unset for [i=1000:1011] object i
unset for [i=1000:1011] label  i
unset arrow 1011
unset multiplot
unset ytics
unset xtics
unset xlabel
unset origin
unset tmargin
unset bmargin
unset lmargin
unset rmargin
set terminal pdf font 'Arial,7' size 3.5in, 1in
set output prefix.'_p2_attacker.pdf'
set multiplot layout 1,2
set border 15
set lmargin 0.5
set rmargin 0.5
set yrange[50.5:63.5]
set y2range[50.5:63.5]
set xrange[30.5:60.5]
set x2range[30.5:60.5]
set palette defined ( \
0   0   0   0  ,\
0.8 0   0   0  ,\
1   1   1   1   ) negative
set xlabel '(a)'
load '../prime-probe/'.prefix.'_victim_cleaned'
plot '../prime-probe/'.prefix.'_attacker_cleaned' matrix with image
load '../prime-probe/'.prefix.'_victim_unset'
set yrange[5.5:18.5]
set y2range[5.5:18.5]
set xrange[8.5:38.5]
set x2range[8.5:38.5]
set palette defined ( \
0	1	0.1	0.1 	,\
0.1	0.2 	0.6	0.3 	,\
0.2	0.85	0.85	0.85	,\
0.4	0.6	0.6	0.6	,\
0.6	0.4	0.4	1	,\
0.7	0.4	0.9	1 	,\
0.8	0.9  	0.9	0.9 	,\
1	1	1	1 	) negative
load '../prime-probe/'.prefix.'_victim_cleaned2'
set xlabel '(b)'
plot '../prime-probe/'.prefix.'_out2' matrix with image
load '../prime-probe/'.prefix.'_victim_unset2'
