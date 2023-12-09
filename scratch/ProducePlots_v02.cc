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

std::string usedAlgorythm = "DT";  // "DT"/"FB"
std::string implementation = "via_MultiQueues";  // "via_NetDevices"/"via_FIFO_QueueDiscs"/"via_MultiQueues"
std::string dir = "./Trace_Plots/";
std::string topology = "2In2Out";  // "Line"/"Incast"/"2In2Out"
std::string traffic_control_type; // "SharedBuffer_DT_v01"/"SharedBuffer_FB_v01"
std::string trace_parameter1_type; // "netDevice_"/"queueDisc_""/"port_"

void
CreateSingle2DPlotFile(size_t portInd, size_t queueInd, std::string priority)  // for a single plot with N data-sets
{
  // Set up some default values for the simulation.
  // std::string dir = "./Trace_Plots/";
  // std::string topology = "2In2Out";  // "Line"/"Incast"/"2In2Out"
  // std::string traffic_control_type; // "DT_FifoQueueDisc_v02"/"FB_FifoQueueDisc_v01"/"SharedBuffer_DT_v01"/"SharedBuffer_FB_v01"
  if (usedAlgorythm.compare("DT") == 0)
  {
    traffic_control_type = "SharedBuffer_DT_v01";
  }
  else if (usedAlgorythm.compare("FB") == 0)
  {
    traffic_control_type = "SharedBuffer_FB_v01";
  } 
  
  //for internal use:
  if (implementation.compare("via_NetDevices") == 0)
  {
    trace_parameter1_type = "netDevice_";
  }
  else if (implementation.compare("via_FIFO_QueueDiscs") == 0)
  {
    trace_parameter1_type = "queueDisc_";
  }
  else if (implementation.compare("via_MultiQueues") == 0)
  {
    trace_parameter1_type = "port_";
  }
  
  std::string trace_parameter1;
  if (implementation.compare("via_MultiQueues") == 0)
  {
    trace_parameter1 = trace_parameter1_type + ToString(portInd) + "_queue_" + ToString(queueInd) + "_" + priority + "PriorityPacketsInQueueTrace";
  }
  else
  {
    trace_parameter1 = trace_parameter1_type + ToString(portInd) + "_" + priority + "PriorityPacketsInQueueTrace";
  }
  
  std::string trace_parameter2 = "TrafficControl" + priority + "PriorityQueueThreshold";
  // can plot as many trace parameters as I wish
  std::string location = dir + topology + "_Topology/" + traffic_control_type + "/" + implementation + "/";
  std::string graphicsFileName = location + "port_" + ToString(portInd) + "_" + priority + ".png";
  std::string plotFileName = location + "port_" + ToString(portInd) + "_" + priority + ".plt";

  std::string plotTitle = priority + " Priority Packets vs Threshold";
  
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
  if (usedAlgorythm.compare("DT") == 0)
  {
    traffic_control_type = "SharedBuffer_DT_v01";
  }
  else if (usedAlgorythm.compare("FB") == 0)
  {
    traffic_control_type = "SharedBuffer_FB_v01";
  } 
  
    //for internal use:
  if (implementation.compare("via_NetDevices") == 0)
  {
    trace_parameter1_type = "netDevice_";
  }
  else if (implementation.compare("via_FIFO_QueueDiscs") == 0)
  {
    trace_parameter1_type = "queueDisc_";
  }
  else if (implementation.compare("via_MultiQueues") == 0)
  {
    trace_parameter1_type = "port_";
  }
  
  std::string trace_parameter1_1;
  std::string trace_parameter1_2;
  std::string trace_parameter1_3;
  std::string trace_parameter1_4;
  
  if (implementation.compare("via_MultiQueues") == 0)
  {
    trace_parameter1_1 = trace_parameter1_type + "0_queue_0_HighPriorityPacketsInQueueTrace";
    trace_parameter1_2 = trace_parameter1_type + "0_queue_1_LowPriorityPacketsInQueueTrace";
    trace_parameter1_3 = trace_parameter1_type + "1_queue_0_HighPriorityPacketsInQueueTrace";
    trace_parameter1_4 = trace_parameter1_type + "1_queue_1_LowPriorityPacketsInQueueTrace";
  }
  else
  {
    trace_parameter1_1 = trace_parameter1_type + "0_HighPriorityPacketsInQueueTrace";
    trace_parameter1_2 = trace_parameter1_type + "0_LowPriorityPacketsInQueueTrace";
    trace_parameter1_3 = trace_parameter1_type + "1_HighPriorityPacketsInQueueTrace";
    trace_parameter1_4 = trace_parameter1_type + "1_LowPriorityPacketsInQueueTrace";
  }

  std::string trace_parameter2_1 = "TrafficControlHighPriorityQueueThreshold";
  std::string trace_parameter2_2 = "TrafficControlLowPriorityQueueThreshold";
  std::string trace_parameter2_3 = "TrafficControlHighPriorityQueueThreshold";
  std::string trace_parameter2_4 = "TrafficControlLowPriorityQueueThreshold";
  
  std::string location = dir + topology + "_Topology/" + traffic_control_type + "/" + implementation + "/";
  std::string graphicsFileName = location + "multiPlot.png";
  std::string plotFileName = location + "multiPlot.plt";

  // std::string plotTitle = "Number of High Priority Packets vs High Priority Threshold in Shared Queue";
  
  GnuplotCollection multiPlot(graphicsFileName);
  multiPlot.SetTitle("High/Low Priority Packets vs Thresholds on each port in Shared Queue");
  
  Gnuplot plot1;
  // plot1.SetTitle("High Priority Packets vs Threshold on NetDevice 0x5569c79dcef0");
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
  
  dataset2_1.SetTitle("Threshold");
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

  if (implementation.compare("via_MultiQueues") == 0 )
  {
    Gnuplot plot2;
    // plot2.SetTitle("Low Priority Packets vs Threshold on NetDevice 0x5569c79dcef0");
    // Set the labels for each axis. xlabel/ylabel
    // plot2.SetLegend("Time[sec]", "PacketsInQueue");

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
    dataset2_2.SetTitle("Threshold");
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

    Gnuplot plot3;
    // plot3.SetTitle("Low Priority Packets vs Threshold on NetDevice 0x5569c79de730");
    // Set the labels for each axis. xlabel/ylabel
    // plot3.SetLegend("Time[sec]", "PacketsInQueue");

    Gnuplot2dDataset dataset1_3;
    dataset1_3.SetTitle("Packets");
    dataset1_3.SetStyle(Gnuplot2dDataset::LINES);
    // load a dat file as data set for plotting
    std::ifstream file13(location + trace_parameter1_3 + ".dat");
    std::cout << "load data from file: " << location + trace_parameter1_3 + ".dat" << std::endl;
    
    x=0;
    y=0;
    
    while (file13 >> x >> y) 
    {
      dataset1_3.Add(x, y);
    }
    // Add the dataset to the plot.
    plot3.AddDataset(dataset1_3);

    Gnuplot2dDataset dataset2_3;
    dataset2_3.SetTitle("Threshold");
    dataset2_3.SetStyle(Gnuplot2dDataset::LINES);
    // load a dat file as data set for plotting
    std::ifstream file23(location + trace_parameter2_3 + ".dat");
    std::cout << "load data from file: " << location + trace_parameter2_3 + ".dat" << std::endl;
    
    x=0;
    y=0;

    while (file23 >> x >> y) 
    {
      dataset2_3.Add(x, y);
    }
    // Add the dataset to the plot.
    plot3.AddDataset(dataset2_3);

    multiPlot.AddPlot(plot3);
  }

  Gnuplot plot4;
  // plot4.SetTitle("Low Priority Packets vs Threshold on NetDevice 0x5569c79de730");
  // Set the labels for each axis. xlabel/ylabel
  // plot4.SetLegend("Time[sec]", "PacketsInQueue");

  Gnuplot2dDataset dataset1_4;
  dataset1_4.SetTitle("Packets");
  dataset1_4.SetStyle(Gnuplot2dDataset::LINES);
  // load a dat file as data set for plotting
  std::ifstream file14(location + trace_parameter1_4 + ".dat");
  std::cout << "load data from file: " << location + trace_parameter1_4 + ".dat" << std::endl;
  
  x=0;
  y=0;

  while (file14 >> x >> y) 
  {
    dataset1_4.Add(x, y);
  }
  // Add the dataset to the plot.
  plot4.AddDataset(dataset1_4);

  Gnuplot2dDataset dataset2_4;
  dataset2_4.SetTitle("Threshold");
  dataset2_4.SetStyle(Gnuplot2dDataset::LINES);
  // load a dat file as data set for plotting
  std::ifstream file24(location + trace_parameter2_4 + ".dat");
  std::cout << "load data from file: " << location + trace_parameter2_4 + ".dat" << std::endl;
  
  x=0;
  y=0;

  while (file24 >> x >> y) 
  {
    dataset2_4.Add(x, y);
  }
  // Add the dataset to the plot.
  plot4.AddDataset(dataset2_4);

  multiPlot.AddPlot(plot4);

  // Open the plot file.
  std::ofstream plotFile(plotFileName);

  // Write the plot file.
  multiPlot.GenerateOutput(plotFile);

  // Close the plot file.
  plotFile.close();

  // command line needs to be in ./ns-3-dev-git$ inorder for the script to produce gnuplot correctly///
  system (("gnuplot " + plotFileName).c_str ());

}

void
CreateAllPlotFiles()  // create a multiplot and all the sub plots sepperatly
{
  // CreateSingle2DPlotFile(0, 0, "High");
  // CreateSingle2DPlotFile(0, 1, "Low");
  // CreateSingle2DPlotFile(1, 0, "High");
  // CreateSingle2DPlotFile(1, 1, "Low");
  CreateSingle2DMultiPlotFile();
}

int main (int argc, char *argv[])
{ 
  // CreateSingle2DPlotFile(1, "Low");
  // CreateSingle2DMultiPlotFile();
  CreateAllPlotFiles();
}