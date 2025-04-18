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

std::string usedAlgorythm = "PredictiveDT";  // "DT"/"FB"/"PredictiveDT"/"PredictiveFB"
std::string implementation = "via_MultiQueues/5_ToS";  // "via_NetDevices/2_ToS"/"via_FIFO_QueueDiscs/2_ToS"/"via_MultiQueues/2_ToS"/"via_MultiQueues/4_ToS"/"via_MultiQueues/5_ToS"
std::string onOffTrafficMode = "Constant"; // "Constant"/"Uniform"/"Normal" 
std::string miceElephantProb = "0.3"; // [0.1:0.1:0.9]/ "VaryingDValues"
std::string testAlphasPair = "15_5"; // format: alphaHigh_alphaLow or "adjustableAlphas". Alpha High/Low = [20, 19, ..., 1, 0.5] 
std::size_t numOfSubQueues = static_cast<size_t>(implementation[implementation.length() - 5] - '0');
std::string dir = "./Trace_Plots/";
std::string topology = "test_Alphas";  // "Line"/"Incast"/"2In2Out"/"test_Alphas"
std::string traffic_control_type; // "SharedBuffer_DT"/"SharedBuffer_FB"
std::string trace_parameter1_type; // "netDevice_"/"queueDisc_""/"port_"

void
CreateSingle2DPlotFile(size_t portInd, size_t queueInd, std::string priority)  // for a single plot with N data-sets
{
  if (usedAlgorythm.compare("DT") == 0 || usedAlgorythm.compare("PredictiveDT") == 0)
  {
    traffic_control_type = "SharedBuffer_DT";
  }
  else if (usedAlgorythm.compare("FB") == 0 || usedAlgorythm.compare("PredictiveFB") == 0)
  {
    traffic_control_type = "SharedBuffer_FB";
  } 
  
  //for internal use:
  if (implementation.compare("via_NetDevices") == 6)
  {
    trace_parameter1_type = "netDevice_";
  }
  else if (implementation.compare("via_FIFO_QueueDiscs") == 6)
  {
    trace_parameter1_type = "queueDisc_";
  }
  else if (implementation.compare("via_MultiQueues") == 6)
  {
    trace_parameter1_type = "port_";
  }
  
  std::string trace_parameter1;
  if (implementation.compare("via_MultiQueues") == 6)
  {
    trace_parameter1 = trace_parameter1_type + ToString(portInd) + "_queue_" + ToString(queueInd) + "_PacketsInQueueTrace";
  }
  else
  {
    trace_parameter1 = trace_parameter1_type + ToString(portInd) + "_" + priority + "PriorityPacketsInQueueTrace";
  }
  
  std::string trace_parameter2 = "TrafficControl" + priority + "PriorityQueueThreshold_" + ToString(portInd);
  
  std::string predictiveIndication = "";
  if (usedAlgorythm.compare("Predictive") == 2)
  {
    predictiveIndication = "Predictive/";
  }
  // std::string location = dir + topology + "_Topology/" + traffic_control_type + "/" + implementation + "/";
  std::string location = dir + topology + "/" + traffic_control_type + "/" + implementation + "/" + onOffTrafficMode + "/" + miceElephantProb + "/" + predictiveIndication;
  if (usedAlgorythm.compare("DT") == 0 || usedAlgorythm.compare("FB") == 0)
  {
    location = location + testAlphasPair + "/";
  }
  std::string graphicsFileName = location + "port_" + ToString(portInd) + "_queue_" + ToString(queueInd) + "_" + priority + ".png";
  std::string plotFileName = location + "port_" + ToString(portInd) + "_queue_" + ToString(queueInd) + "_" + priority + ".plt";

  std::string plotTitle = "port " + ToString(portInd) + " queue " + ToString(queueInd) + " " + priority + " Priority Packets vs Threshold";
  
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
  dataset1.SetStyle(Gnuplot2dDataset::LINES);
  // load a dat file as data set for plotting
  std::ifstream file1(location + trace_parameter1 + ".dat");
  // std::cout << "load data from file: " << location + trace_parameter1 + ".dat" << std::endl;
  
  double x, y;
  x = 0;
  y = 0;

  while (file1 >> x >> y) 
  {
    dataset1.Add(x, y);
  }
  // Add the dataset to the plot.
  plot.AddDataset(dataset1);

  dataset2.SetTitle(trace_parameter2);
  dataset2.SetStyle(Gnuplot2dDataset::LINES);
  // load a dat file as data set for plotting
  std::ifstream file2(location + trace_parameter2 + ".dat");
  // std::cout << "load data from file: " << location + trace_parameter2 + ".dat" << std::endl;
  x = 0;
  y = 0;
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
  if (usedAlgorythm.compare("DT") == 0 || usedAlgorythm.compare("PredictiveDT") == 0)
  {
    traffic_control_type = "SharedBuffer_DT";
  }
  else if (usedAlgorythm.compare("FB") == 0 || usedAlgorythm.compare("PredictiveFB") == 0)
  {
    traffic_control_type = "SharedBuffer_FB";
  }

  
  //for internal use:
  if (implementation.compare("via_NetDevices") == 6)
  {
    trace_parameter1_type = "netDevice_";
  }
  else if (implementation.compare("via_FIFO_QueueDiscs") == 6)
  {
    trace_parameter1_type = "queueDisc_";
  }
  else if (implementation.compare("via_MultiQueues") == 6)
  {
    trace_parameter1_type = "port_";
  }
  
  std::map<std::string, std::string> trace_parameter1_array;
  std::map<std::string, std::string> trace_parameter2_array;

  bool unequalNum = false; // a flag that's raised if the number of High/Low priority OnOff applications created is diffeent for each Port
  if ((int(std::stod(miceElephantProb) * 10) % 2) != 0)
  {
    unequalNum = true;
  }

  for (size_t i = 0; i < 2; i++) // iterate over port
  {
    if (implementation.compare("via_NetDevices") == 6 || implementation.compare("via_FIFO_QueueDiscs") == 6)
    {
      std::string trace_parameter1_name = "trace_parameter1_" + ToString(i);
      std::string trace_parameter2_name = "trace_parameter2_" + ToString(i);
      if (i==0)
      {
        std::string trace_parameter1_value = trace_parameter1_type + ToString(i) + "_HighPriorityPacketsInQueueTrace";
        trace_parameter1_array[trace_parameter1_name] = trace_parameter1_value;

        std::string trace_parameter2_value = "TrafficControlHighPriorityQueueThreshold_" + ToString(i);
        trace_parameter2_array[trace_parameter2_name] = trace_parameter2_value;
      }
      else
      {
        std::string trace_parameter1_value = trace_parameter1_type + ToString(i) + "_LowPriorityPacketsInQueueTrace";
        trace_parameter1_array[trace_parameter1_name] = trace_parameter1_value;

        std::string trace_parameter2_value = "TrafficControlLowPriorityQueueThreshold_" + ToString(i);
        trace_parameter2_array[trace_parameter2_name] = trace_parameter2_value;
      }
    }
    else if (implementation.compare("via_MultiQueues") == 6)
    {
      for (size_t j = 0; j < numOfSubQueues; j++)  // iterate over sub-queue
      {
        std::string trace_parameter1_name = "trace_parameter1_" + ToString(i) + "_" + ToString(j);
        std::string trace_parameter2_name = "trace_parameter2_" + ToString(i) + "_" + ToString(j);
        if (unequalNum) 
        {
          if (i == 0 && j < int(ceil(10*std::stod(miceElephantProb)/2)) || i == 1 && j < int(floor(10*std::stod(miceElephantProb)/2)))
            {
              std::string trace_parameter1_value = trace_parameter1_type + ToString(i) + "_queue_" + ToString(j) + "_PacketsInQueueTrace";
              trace_parameter1_array[trace_parameter1_name] = trace_parameter1_value;

              std::string trace_parameter2_value = "TrafficControlHighPriorityQueueThreshold_" + ToString(i);
              trace_parameter2_array[trace_parameter2_name] = trace_parameter2_value;
            }
          else
          {
            std::string trace_parameter1_value = trace_parameter1_type + ToString(i) + "_queue_" + ToString(j) + "_PacketsInQueueTrace";
            trace_parameter1_array[trace_parameter1_name] = trace_parameter1_value;
            
            std::string trace_parameter2_value = "TrafficControlLowPriorityQueueThreshold_" + ToString(i);
            trace_parameter2_array[trace_parameter2_name] = trace_parameter2_value;
          }
        }
        else
        {
          if (j < int(10*std::stod(miceElephantProb)/2))
          {
            std::string trace_parameter1_value = trace_parameter1_type + ToString(i) + "_queue_" + ToString(j) + "_PacketsInQueueTrace";
            trace_parameter1_array[trace_parameter1_name] = trace_parameter1_value;

            std::string trace_parameter2_value = "TrafficControlHighPriorityQueueThreshold_" + ToString(i);
            trace_parameter2_array[trace_parameter2_name] = trace_parameter2_value;
          }
          else
          {
            std::string trace_parameter1_value = trace_parameter1_type + ToString(i) + "_queue_" + ToString(j) + "_PacketsInQueueTrace";
            trace_parameter1_array[trace_parameter1_name] = trace_parameter1_value;
            
            std::string trace_parameter2_value = "TrafficControlLowPriorityQueueThreshold_" + ToString(i);
            trace_parameter2_array[trace_parameter2_name] = trace_parameter2_value;
          }
        }
      }
    }
  }
  
  std::string predictiveIndication = "";
  if (usedAlgorythm.compare("Predictive") == 2)
  {
    predictiveIndication = "Predictive/";
  }
  
  // std::string location = dir + topology + "_Topology/" + traffic_control_type + "/" + implementation + "/";
  std::string location = dir + topology + "/" + traffic_control_type + "/" + implementation + "/" + onOffTrafficMode + "/" + miceElephantProb + "/" + predictiveIndication;
  if (usedAlgorythm.compare("DT") == 0 || usedAlgorythm.compare("FB") == 0)
  {
    location = location + testAlphasPair + "/";
  }
  std::string graphicsFileName = location + "multiPlot.png";
  std::string plotFileName = location + "multiPlot.plt";
  
  GnuplotCollection multiPlot(graphicsFileName);
  multiPlot.SetTitle("High/Low Priority Packets vs Thresholds on each port in Shared Queue");
  
  for (size_t i = 0; i < 2; i++) // iterate over port
  {
    if (implementation.compare("via_NetDevices") == 6 || implementation.compare("via_FIFO_QueueDiscs") == 6)
    {
      Gnuplot plot;
      Gnuplot2dDataset dataset1;
      
      dataset1.SetTitle("Packets");
      dataset1.SetStyle(Gnuplot2dDataset::LINES);
      // load a dat file as data set for plotting
      std::ifstream file1(location + trace_parameter1_array["trace_parameter1_" + ToString(i)] + ".dat");
      std::cout << "load data from file: " << location + trace_parameter1_array["trace_parameter1_" + ToString(i)] + ".dat" << std::endl;

      double x, y;
      x=0;
      y=0;

      while (file1 >> x >> y) 
      {
        dataset1.Add(x, y);
      }

      // Add the dataset to the plot.
      plot.AddDataset(dataset1);

      // add the desired trace parameters to plot
      Gnuplot2dDataset dataset2;
      
      dataset2.SetTitle("Threshold");
      dataset2.SetStyle(Gnuplot2dDataset::LINES);
      // load a dat file as data set for plotting
      std::ifstream file2(location + trace_parameter2_array["trace_parameter2_" + ToString(i)] + ".dat");
      std::cout << "load data from file: " << location + trace_parameter2_array["trace_parameter2_" + ToString(i)] + ".dat" << std::endl;
      
      x = 0;
      y = 0;

      while (file2 >> x >> y) 
      {
        dataset2.Add(x, y);
      }
      // Add the dataset to the plot.
      plot.AddDataset(dataset2);

      // add subplot to the total plot
      multiPlot.AddPlot(plot);
    }
    else if (implementation.compare("via_MultiQueues") == 6)
    {
      for (size_t j = 0; j < numOfSubQueues; j++)  // iterate over sub-queue
        {
          Gnuplot plot;
          Gnuplot2dDataset dataset1;
          
          dataset1.SetTitle("Packets");
          dataset1.SetStyle(Gnuplot2dDataset::LINES);
          // load a dat file as data set for plotting
          std::ifstream file1(location + trace_parameter1_array["trace_parameter1_" + ToString(i) + "_" + ToString(j)] + ".dat");
          std::cout << "load data from file: " << location + trace_parameter1_array["trace_parameter1_" + ToString(i) + "_" + ToString(j)] + ".dat" << std::endl;

          double x, y;
          x=0;
          y=0;

          while (file1 >> x >> y) 
          {
            dataset1.Add(x, y);
          }

          // Add the dataset to the plot.
          plot.AddDataset(dataset1);

          // add the desired trace parameters to plot
          Gnuplot2dDataset dataset2;
          
          dataset2.SetTitle("Threshold");
          dataset2.SetStyle(Gnuplot2dDataset::LINES);
          // load a dat file as data set for plotting
          std::ifstream file2(location + trace_parameter2_array["trace_parameter2_" + ToString(i) + "_" + ToString(j)] + ".dat");
          std::cout << "load data from file: " << location + trace_parameter2_array["trace_parameter2_" + ToString(i) + "_" + ToString(j)] + ".dat" << std::endl;
          
          x=0;
          y=0;

          while (file2 >> x >> y) 
          {
            dataset2.Add(x, y);
          }
          // Add the dataset to the plot.
          plot.AddDataset(dataset2);

          // add subplot to the total plot
          multiPlot.AddPlot(plot);
        }
      }
    }
    
  
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
  bool unequalNum = false; // a flag that's raised if the number of High/Low priority OnOff applications created is diffeent for each Port
  if ((int(std::stod(miceElephantProb) * 10) % 2) != 0)
  {
    unequalNum = true;
  }

  for (size_t i = 0; i < 2; i++)
  {
    for (size_t j = 0; j < numOfSubQueues; j++)
    {
      if (unequalNum) 
      {
        if (i == 0 && j < int(ceil(10*std::stod(miceElephantProb)/2)) || i == 1 && j < int(floor(10*std::stod(miceElephantProb)/2)))
          {
            CreateSingle2DPlotFile(i, j, "High");
          }
        else
        {
          CreateSingle2DPlotFile(i, j, "Low");
        }  
      }
      else
      {
        if (j < int(10*std::stod(miceElephantProb)/2))
        {
          CreateSingle2DPlotFile(i, j, "High");
        }
        else
        {
          CreateSingle2DPlotFile(i, j, "Low");
        }
        
      }
      
    }
  }
  CreateSingle2DMultiPlotFile();
}

int main (int argc, char *argv[])
{ 
  // CreateSingle2DPlotFile(0, 1, "High");
  // CreateSingle2DMultiPlotFile();
  CreateAllPlotFiles();

  return 0;
}