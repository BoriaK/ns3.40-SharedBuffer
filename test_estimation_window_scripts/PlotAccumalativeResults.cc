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


// std::string
// ToString (uint32_t value)
// {
//   std::stringstream ss;
//   ss << value;
//   return ss.str();
// }


void
CreateSingle2DPlotFile(std::string someImplementation, std::string onoff_traffic_mode, std::string someUsedAlgorythm)  // for a single plot with N data-sets
{
  std::string gnuPlotFileLocation = "./test_alphas_scripts/GnuPlot_" + onoff_traffic_mode + "/";

  // // Set up some default values for the simulation.
  std::string trace_parameter = someUsedAlgorythm + "_TestAccumulativeStatistics";
  std::string gnuPlotFileName = gnuPlotFileLocation + trace_parameter + "_" + someImplementation + ".plt";

  // command line needs to be in ./ns-3-dev-git$ inorder for the script to produce gnuplot correctly///
  system (("gnuplot " + gnuPlotFileName).c_str ());

}

// Function template to accept a reference to an array of any size
template <size_t N>
void
CreateMultiple2DPlotFiles(std::string someImplementation, std::string onoff_traffic_mode, const std::string (&someUsedAlgorythmArray)[N])  // for a single plot with N data-sets
{
  for (size_t i = 0; i < N; i++)
  {
    CreateSingle2DPlotFile(someImplementation, onoff_traffic_mode, someUsedAlgorythmArray[i]);
  }
}

int main (int argc, char *argv[])
{ 
  std::string implementation = "MQueue_5ToS";  // "FIFO"/"MQueue_2ToS"/"MQueue_5ToS"
  std::string onOffTrafficMode = "Constant"; // "Constant"/"Uniform"/"Normal"                                      
  // for a single plot: /////////////////////                                                  
  std::string usedAlgorythm = "DT_VaryingDValues"; 
  // Options: 
  // "DT"/"FB"/"All"
  // "FB_0.1"/"FB_0.2"/"FB_0.3"/"FB_0.4"/"FB_0.5"/"FB_0.6"/"FB_0.7"/"FB_0.8"/"FB_0.9"/"FB_All_D"/"FB_VaryingDValues"
  // "DT_0.1"/"DT_0.2"/"DT_0.3"/"DT_0.4"/"DT_0.5"/"DT_0.6"/"DT_0.7"/"DT_0.8"/"DT_0.9"/"DT_All_D"/"DT_VaryingDValues"
  // "DT_vs_FB_0.1"
  CreateSingle2DPlotFile(implementation, onOffTrafficMode, usedAlgorythm);
  ///////////////////////////////////////////
  
  // for multiple different plots:
  std::string usedAlgorythmArray[] = {"DT_0.1","DT_0.2","DT_0.3","DT_0.4","DT_0.5","DT_0.6","DT_0.7","DT_0.8","DT_0.9","DT_All_D"};
  // std::string usedAlgorythmArray[] = {"FB_0.1","FB_0.2","FB_0.3","FB_0.4","FB_0.5","FB_0.6","FB_0.7","FB_0.8","FB_0.9","FB_All_D"};
  // std::string usedAlgorythmArray[] = {"DT_vs_FB_0.1","DT_vs_FB_0.2","DT_vs_FB_0.3","DT_vs_FB_0.4","DT_vs_FB_0.5","DT_vs_FB_0.6","DT_vs_FB_0.7","DT_vs_FB_0.8","DT_vs_FB_0.9"};
  // CreateMultiple2DPlotFiles(implementation, onOffTrafficMode, usedAlgorythmArray);
}