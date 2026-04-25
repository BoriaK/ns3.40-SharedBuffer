set terminal png
set output "/home/boris/workspace/ns-allinone-3.40/ns-3.40/Trace_Plots/1In1Out/Predictive/Position_0.5/Length_0.4/SharedBuffer_DT/via_MultiQueues/2_ToS/prioSteadyOn/0.2/Predictive/TCP/0.02//port_0_queue_0_High.png"
set title "port 0 queue 0 High Priority Packets vs Threshold"
set xlabel "Time[sec]"
set ylabel "PacketsInQueue"
plot "-"  title "High priority enqueued Packets" with lines, 
2.50011 1
2.50854 0
2.52011 1
2.52671 0
e
e
