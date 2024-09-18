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
#include <stdint.h>

#include "ns3/core-module.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"


using namespace ns3;

template <size_t N>
void collectData(std::string path, std::string onoff_traffic_mode, const std::string (&mice_elephant_prob_array)[N])
{
  size_t m_endPos = path.find("/via_MultiQueues");
  std::string m_someUsedAlgorythm = path.substr(m_endPos - 2, 2);
  std::string line = ""; // empty line
  
  // create a new .dat file that would store all the data from other data sets:
  
  // Open the file in truncation mode to clear its content
  std::ofstream dataFile(path + onoff_traffic_mode + "/Complete_Predictive" + m_someUsedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::trunc);
  dataFile.close();  // Close the file after truncation
  
  // Re-open the file in append mode
  dataFile.open(path + onoff_traffic_mode + "/Complete_Predictive" + m_someUsedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
  for (size_t i = 0; i < N; i++)
  {
    std::ifstream file(path + onoff_traffic_mode + "/" + mice_elephant_prob_array[i] + "/Predictive" + m_someUsedAlgorythm + "_TestAccumulativeStatistics.dat");
    std::getline(file, line);
    dataFile << line << std::endl;
    file.close();
  }
  dataFile.close();
}


int main (int argc, char *argv[])
{ 
  std::string testType = "test_Estimation_Window"; // "test_Alphas"/"test_Estimation_Window_Width"
  std::string windowPossition = "0.5";  // ""/"0.25"/"0.5"/0.75 
  std::string Tau = "0.04"; // "0.02"/"0.03"/"0.04"/"0.05"
  std::string filePath = "Trace_Plots/" + testType + "/Position_" + windowPossition + "/Length_" + Tau + "/TestStats/SharedBuffer_DT/via_MultiQueues/5_ToS/";
  std::string onOffTrafficMode = "Uniform"; // "Constant"/"Uniform"/"Normal"
  
  // std::string miceElephantProbArray[] = {"0.1"};
  std::string miceElephantProbArray[] = {"0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9"};
  
  collectData(filePath, onOffTrafficMode, miceElephantProbArray);
}