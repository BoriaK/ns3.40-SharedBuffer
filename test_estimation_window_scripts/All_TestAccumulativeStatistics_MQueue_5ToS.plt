set terminal png
set output "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/All_TestAccumulativeStatistics.png"
# set title "total packets dropped"
set xlabel "Alpha High/Low"
set ylabel "Dropped Packets"

# Set the data file separator (whitespace in this case)
set datafile separator whitespace

# Use multiplot to arrange the plots in the same graph
set multiplot

set xtics 1  # This sets the distance between xtics to 1

plot "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/DT_TestAccumulativeStatistics.dat" using 0:2:xticlabels(1) with linespoints title "Total Packets Dropped DT", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/DT_TestAccumulativeStatistics.dat" using 0:3:xticlabels(1) with linespoints title "High Priority Packets Dropped DT", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/DT_TestAccumulativeStatistics.dat" using 0:4:xticlabels(1) with linespoints title "Low Priority Packets Dropped DT", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/FB_TestAccumulativeStatistics.dat" using 0:2:xticlabels(1) with linespoints title "Total Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/FB_TestAccumulativeStatistics.dat" using 0:3:xticlabels(1) with linespoints title "High Priority Packets Dropped FB", \
     "./Trace_Plots/test_Alphas/TestStats/via_MultiQueues/5_ToS/FB_TestAccumulativeStatistics.dat" using 0:4:xticlabels(1) with linespoints title "Low Priority Packets Dropped FB", \


# Set legend properties
set key autotitle columnheader  # Use the column header as legend title
set key top left                # Position the legend at the top left of the graph

# Reset multiplot to exit the multiplot mode
unset multiplot