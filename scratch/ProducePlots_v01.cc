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


using namespace ns3;



int main (int argc, char *argv[])
{ 
  // // Set up some default values for the simulation.
  // std::string dir = "./Trace_Plots/";
  // std::string topology = "2In2Out";  // "Line"/"Incast"/"2In2Out"
  // std::string traffic_control_type = "SharedBuffer_DT_v01"; // "DT_FifoQueueDisc_v02"/"FB_FifoQueueDisc_v01"
  
  // Set up some default values for the simulation.
  std::string dir = "./Trace_Plots/";
  std::string topology = "2In2Out";  // "Line"/"Incast"/"2In2Out"
  std::string traffic_control_type = "SharedBuffer_DT_v01"; // "DT_FifoQueueDisc_v02"/"FB_FifoQueueDisc_v01"
  // std::string trace_parameter1 = "TrafficControlHighPriorityPacketsInQueueTrace";
  // std::string trace_parameter2 = "TrafficControlHighPriorityQueueThreshold";
  std::string location = dir + topology + "_Topology/" + traffic_control_type + "/";
  std::string graphicsFileName = location + "plot.png";
  std::string plotFileName = location + "multiPlot.plt";

  // command line needs to be in ./scratch/ inorder for the script to produce gnuplot correctly///
  system (("gnuplot " + plotFileName).c_str ());
}