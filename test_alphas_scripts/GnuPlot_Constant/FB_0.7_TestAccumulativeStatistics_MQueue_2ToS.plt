set terminal png
set output "./Trace_Plots/test_Alphas/TestStats/SharedBuffer_FB_v01/via_MultiQueues/2_ToS/Constant/FB_0.7_TestAccumulativeStatistics.png"
# set title "total packets dropped"
set xlabel "Alpha High/Low"
set ylabel "Dropped Packets"

# Set the data file separator (whitespace in this case)
set datafile separator whitespace

# Use multiplot to arrange the plots in the same graph
set multiplot

set xtics 1  # This sets the distance between xtics to 1

plot "./Trace_Plots/test_Alphas/TestStats/SharedBuffer_FB_v01/via_MultiQueues/2_ToS/Constant/0.7/FB_TestAccumulativeStatistics.dat" using 0:2:xticlabels(1) with linespoints title "0.7 Total Packets Dropped", \
     "./Trace_Plots/test_Alphas/TestStats/SharedBuffer_FB_v01/via_MultiQueues/2_ToS/Constant/0.7/FB_TestAccumulativeStatistics.dat" using 0:3:xticlabels(1) with linespoints title "0.7 High Priority Packets Dropped", \
     "./Trace_Plots/test_Alphas/TestStats/SharedBuffer_FB_v01/via_MultiQueues/2_ToS/Constant/0.7/FB_TestAccumulativeStatistics.dat" using 0:4:xticlabels(1) with linespoints title "0.7 Low Priority Packets Dropped", \

# Set legend properties
set key autotitle columnheader  # Use the column header as legend title
set key top left                # Position the legend at the top left of the graph

# Reset multiplot to exit the multiplot mode
unset multiplot