/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/gnuplot.h"


using namespace ns3;


std::string
ToString (uint32_t value)
{
  std::stringstream ss;
  ss << value;
  return ss.str();
}

// .dat files path:
std::string path = "./Trace_Plots/2In2Out_Topology/SharedBuffer_PredictiveFB_v01/via_MultiQueues/2_ToS";

void
CreateSingle2DPlotFile(std::string priority)  // for a single plot with N data-sets
{ 
  std::string location = path + "/";
  std::string trace_parameter1;
  std::string trace_parameter2;
  std::string graphicsFileName;
  std::string plotFileName;
  std::string plotTitle;

  if (priority.length() > 0)
  {
    trace_parameter1 = "TrafficControl" + priority + "Priority" + "PacketsInQueueTrace";
    trace_parameter2 = "TrafficControlPredict" + priority + "Priority" + "PacketsInQueueTrace";
    graphicsFileName = location + priority + "Priority" + "TC_Packets_vs_PredictivePackets_InQueue.png";
    plotFileName = location + priority + "Priority" + "TC_Packets_vs_PredictivePackets_InQueue.plt";
    plotTitle = priority + " Priority " + "Packets vs Predictive Packets in Shared Queue";
  }
  else
  {
    trace_parameter1 = "TrafficControlPacketsInQueueTrace";
    trace_parameter2 = "TrafficControlPredictPacketsInQueueTrace";
    graphicsFileName = location + "TC_Packets_vs_PredictivePackets_InQueue.png";
    plotFileName = location + "TC_Packets_vs_PredictivePackets_InQueue.plt";
    plotTitle = "Packets vs Predictive Packets in Shared Queue";
  }
  
  // std::string dataTitle = trace_parameter;

  Gnuplot plot(graphicsFileName);
  plot.SetTitle(plotTitle);
  // Make the graphics file, which the plot file will create when it
  // is used with Gnuplot, be a PNG file.
  plot.SetTerminal("png");
  
  // Set the labels for each axis. xlabel/ylabel
  plot.SetLegend("Time[sec]", "PacketsInQueue");

  // add the desired trace parameters to plot
  Gnuplot2dDataset dataset1, dataset2;
  
  dataset1.SetTitle(trace_parameter1);
  dataset1.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  // load a dat file as data set for plotting
  std::ifstream file1(location + trace_parameter1 + ".dat");
  // std::cout << "load data from file: " << location + trace_parameter1 + ".dat" << std::endl;
  
  double x, y;
  x=0;
  y=0;

  while (file1 >> x >> y) 
  {
    dataset1.Add(x, y);
  }
  // Add the dataset to the plot.
  plot.AddDataset(dataset1);

  dataset2.SetTitle(trace_parameter2);
  dataset2.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  // load a dat file as data set for plotting
  std::ifstream file2(location + trace_parameter2 + ".dat");
  // std::cout << "load data from file: " << location + trace_parameter2 + ".dat" << std::endl;
  x=0;
  y=0;
  while (file2 >> x >> y) 
  {
    dataset2.Add(x, y);
  }
  // Add the dataset to the plot.
  plot.AddDataset(dataset2);

  // Open the plot file.
  std::ofstream plotFile(plotFileName);

  // Write the plot file.
  plot.GenerateOutput(plotFile);

  // Close the plot file.
  plotFile.close();

  // command line needs to be in ./ns-3-dev-git$ inorder for the script to produce gnuplot correctly///
  system (("gnuplot " + plotFileName).c_str ());
}

void
CreateSingle2DMultiPlotFile()  // for a multiplot, with N data-sets each
{
  // Set up some default values for the simulation.
  // std::string dir = "./Trace_Plots/";
  // std::string topology = "2In2Out";  // "Line"/"Incast"/"2In2Out"
  // std::string traffic_control_type; // "DT_FifoQueueDisc_v02"/"FB_FifoQueueDisc_v01"/"SharedBuffer_DT_v01"/"SharedBuffer_FB_v01"
  
  std::string location = path + "/";  
  std::string graphicsFileName = location + "multiPlot.png";
  std::string plotFileName = location + "multiPlot.plt";

  std::string trace_parameter1_1 = "TrafficControlHighPriorityPacketsInQueueTrace";
  std::string trace_parameter1_2 = "TrafficControlLowPriorityPacketsInQueueTrace";
  std::string trace_parameter2_1 = "TrafficControlPredictHighPriorityPacketsInQueueTrace";
  std::string trace_parameter2_2 = "TrafficControlPredictLowPriorityPacketsInQueueTrace";
  
  GnuplotCollection multiPlot(graphicsFileName);
  multiPlot.SetTitle("High/Low Priority Packets vs Predictive Packets in Shared Queue");
  
  Gnuplot plot1;
  plot1.SetTitle("High Priority Packets");
  // Set the labels for each axis. xlabel/ylabel
  // plot1.SetLegend("Time[sec]", "PacketsInQueue");

  // add the desired trace parameters to plot
  Gnuplot2dDataset dataset1_1;
  
  dataset1_1.SetTitle("Packets");
  dataset1_1.SetStyle(Gnuplot2dDataset::LINES);
  // load a dat file as data set for plotting
  std::ifstream file11(location + trace_parameter1_1 + ".dat");
  std::cout << "load data from file: " << location + trace_parameter1_1 + ".dat" << std::endl;
  
  double x, y;
  x=0;
  y=0;

  while (file11 >> x >> y) 
  {
    dataset1_1.Add(x, y);
  }

  // Add the dataset to the plot.
  plot1.AddDataset(dataset1_1);

  // add the desired trace parameters to plot
  Gnuplot2dDataset dataset2_1;
  
  dataset2_1.SetTitle("PredictivePackets");
  dataset2_1.SetStyle(Gnuplot2dDataset::LINES);
  // load a dat file as data set for plotting
  std::ifstream file21(location + trace_parameter2_1 + ".dat");
  std::cout << "load data from file: " << location + trace_parameter2_1 + ".dat" << std::endl;
  
  x=0;
  y=0;

  while (file21 >> x >> y) 
  {
    dataset2_1.Add(x, y);
  }
  // Add the dataset to the plot.
  plot1.AddDataset(dataset2_1);

  // add the first subplot to the total plot
  multiPlot.AddPlot(plot1);

  Gnuplot plot2;
  plot2.SetTitle("Low Priority Packets");
  // Set the labels for each axis. xlabel/ylabel
  // plot4.SetLegend("Time[sec]", "PacketsInQueue");

  Gnuplot2dDataset dataset1_2;
  dataset1_2.SetTitle("Packets");
  dataset1_2.SetStyle(Gnuplot2dDataset::LINES);
  // load a dat file as data set for plotting
  std::ifstream file12(location + trace_parameter1_2 + ".dat");
  std::cout << "load data from file: " << location + trace_parameter1_2 + ".dat" << std::endl;
  
  x=0;
  y=0;

  while (file12 >> x >> y) 
  {
    dataset1_2.Add(x, y);
  }
  // Add the dataset to the plot.
  plot2.AddDataset(dataset1_2);

  Gnuplot2dDataset dataset2_2;
  dataset2_2.SetTitle("PredictivePackets");
  dataset2_2.SetStyle(Gnuplot2dDataset::LINES);
  // load a dat file as data set for plotting
  std::ifstream file22(location + trace_parameter2_2 + ".dat");
  std::cout << "load data from file: " << location + trace_parameter2_2 + ".dat" << std::endl;
  
  x=0;
  y=0;

  while (file22 >> x >> y) 
  {
    dataset2_2.Add(x, y);
  }
  // Add the dataset to the plot.
  plot2.AddDataset(dataset2_2);

  multiPlot.AddPlot(plot2);

  // Open the plot file.
  std::ofstream plotFile(plotFileName);

  // Write the plot file.
  multiPlot.GenerateOutput(plotFile);

  // Close the plot file.
  plotFile.close();

  // command line needs to be in ./ns-3-dev-git$ inorder for the script to produce gnuplot correctly///
  system (("gnuplot " + plotFileName).c_str ());

}

int main (int argc, char *argv[])
{ 
  // CreateSingle2DPlotFile("Low");
  CreateSingle2DMultiPlotFile();
}