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
CreateSingle2DPlotFile(std::string someUsedAlgorythm, std::string someImplementation)  // for a single plot with N data-sets
{
  std::string gnuPlotFileLocation = "./test_alphas_scripts/";

  // // Set up some default values for the simulation.
  std::string trace_parameter = someUsedAlgorythm + "_TestAccumulativeStatistics";
  std::string gnuPlotFileName = gnuPlotFileLocation + trace_parameter + "_" + someImplementation + ".plt";

  // command line needs to be in ./ns-3-dev-git$ inorder for the script to produce gnuplot correctly///
  system (("gnuplot " + gnuPlotFileName).c_str ());

}

// Function template to accept a reference to an array of any size
template <size_t N>
void
CreateMultiple2DPlotFiles(const std::string (&someUsedAlgorythmArray)[N], std::string someImplementation)  // for a single plot with N data-sets
{
  for (size_t i = 0; i < N; i++)
  {
    CreateSingle2DPlotFile(someUsedAlgorythmArray[i], someImplementation);
  }
}

int main (int argc, char *argv[])
{ 
                                        // "FB_0.1"/"FB_0.2"/"FB_0.3"/"FB_0.4"/"FB_0.5"/"FB_0.6"/"FB_0.7"/"FB_0.8"/"FB_0.9"
  std::string usedAlgorythm = "FB_All_D";  // "DT"/"FB"/"All"/"FB_All_D"
  std::string implementation = "MQueue_5ToS";  // "FIFO"/"MQueue_2ToS"/"MQueue_5ToS"
  // CreateSingle2DPlotFile(usedAlgorythm, implementation);
  
  // for multiple different plots
  // const int arraySize = 9; // insert the number of different elements to plot
  // std::array<std::string, arraySize> usedAlgorythmArray = {"FB_0.1","FB_0.2","FB_0.3","FB_0.4","FB_0.5","FB_0.6","FB_0.7","FB_0.8","FB_0.9"};
  std::string usedAlgorythmArray[] = {"FB_0.1","FB_0.2","FB_0.3","FB_0.4","FB_0.5","FB_0.6","FB_0.7","FB_0.8","FB_0.9","FB_All_D"};
  CreateMultiple2DPlotFiles(usedAlgorythmArray, implementation);
}