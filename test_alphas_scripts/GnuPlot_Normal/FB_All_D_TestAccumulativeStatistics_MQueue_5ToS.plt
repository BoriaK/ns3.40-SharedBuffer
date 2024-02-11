set terminal png
set output "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/FB_All_D_TestAccumulativeStatistics.png"
# set title "total packets dropped"
set xlabel "Alpha High/Low"
set ylabel "Dropped Packets"

# Set the data file separator (whitespace in this case)
set datafile separator whitespace

# Use multiplot to arrange the plots in the same graph
set multiplot

set xtics 1  # This sets the distance between xtics to 1

plot "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.1/FB_TestAccumulativeStatistics.dat" using 0:2:xticlabels(1) with linespoints lc rgb "blue" title "0.1 Total Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.1/FB_TestAccumulativeStatistics.dat" using 0:3:xticlabels(1) with linespoints lc rgb "blue" title "0.1 High Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.1/FB_TestAccumulativeStatistics.dat" using 0:4:xticlabels(1) with linespoints lc rgb "blue" title "0.1 Low Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.2/FB_TestAccumulativeStatistics.dat" using 0:2:xticlabels(1) with linespoints lc rgb "green" title "0.2 Total Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.2/FB_TestAccumulativeStatistics.dat" using 0:3:xticlabels(1) with linespoints lc rgb "green" title "0.2 High Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.2/FB_TestAccumulativeStatistics.dat" using 0:4:xticlabels(1) with linespoints lc rgb "green" title "0.2 Low Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.3/FB_TestAccumulativeStatistics.dat" using 0:2:xticlabels(1) with linespoints lc rgb "cyan" title "0.3 Total Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.3/FB_TestAccumulativeStatistics.dat" using 0:3:xticlabels(1) with linespoints lc rgb "cyan" title "0.3 High Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.3/FB_TestAccumulativeStatistics.dat" using 0:4:xticlabels(1) with linespoints lc rgb "cyan" title "0.3 Low Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.4/FB_TestAccumulativeStatistics.dat" using 0:2:xticlabels(1) with linespoints lc rgb "brown" title "0.4 Total Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.4/FB_TestAccumulativeStatistics.dat" using 0:3:xticlabels(1) with linespoints lc rgb "brown" title "0.4 High Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.4/FB_TestAccumulativeStatistics.dat" using 0:4:xticlabels(1) with linespoints lc rgb "brown" title "0.4 Low Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.5/FB_TestAccumulativeStatistics.dat" using 0:2:xticlabels(1) with linespoints lc rgb "yellow" title "0.5 Total Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.5/FB_TestAccumulativeStatistics.dat" using 0:3:xticlabels(1) with linespoints lc rgb "yellow" title "0.5 High Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.5/FB_TestAccumulativeStatistics.dat" using 0:4:xticlabels(1) with linespoints lc rgb "yellow" title "0.5 Low Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.6/FB_TestAccumulativeStatistics.dat" using 0:2:xticlabels(1) with linespoints lc rgb "red" title "0.6 Total Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.6/FB_TestAccumulativeStatistics.dat" using 0:3:xticlabels(1) with linespoints lc rgb "red" title "0.6 High Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.6/FB_TestAccumulativeStatistics.dat" using 0:4:xticlabels(1) with linespoints lc rgb "red" title "0.6 Low Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.7/FB_TestAccumulativeStatistics.dat" using 0:2:xticlabels(1) with linespoints lc rgb "orange" title "0.7 Total Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.7/FB_TestAccumulativeStatistics.dat" using 0:3:xticlabels(1) with linespoints lc rgb "orange" title "0.7 High Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.7/FB_TestAccumulativeStatistics.dat" using 0:4:xticlabels(1) with linespoints lc rgb "orange" title "0.7 Low Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.8/FB_TestAccumulativeStatistics.dat" using 0:2:xticlabels(1) with linespoints lc rgb "gray" title "0.8 Total Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.8/FB_TestAccumulativeStatistics.dat" using 0:3:xticlabels(1) with linespoints lc rgb "gray" title "0.8 High Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.8/FB_TestAccumulativeStatistics.dat" using 0:4:xticlabels(1) with linespoints lc rgb "gray" title "0.8 Low Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.9/FB_TestAccumulativeStatistics.dat" using 0:2:xticlabels(1) with linespoints lc rgb "purple" title "0.9 Total Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.9/FB_TestAccumulativeStatistics.dat" using 0:3:xticlabels(1) with linespoints lc rgb "purple" title "0.9 High Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/Normal/0.9/FB_TestAccumulativeStatistics.dat" using 0:4:xticlabels(1) with linespoints lc rgb "purple" title "0.9 Low Priority Packets Dropped FB", \


# Set legend properties
set key autotitle columnheader  # Use the column header as legend title
set key top left                # Position the legend at the top left of the graph

# Reset multiplot to exit the multiplot mode
unset multiplot