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
#include <unistd.h>
#include <cstdlib>
#include <cstdio>

#include "ns3/core-module.h"
#include "ns3/gnuplot.h"


using namespace ns3;

std::string GetWorkspaceRoot()
{
    char* cwd = getcwd(nullptr, 0);
    if (cwd == nullptr)
    {
        return "./";
    }
    std::string wsRoot(cwd);
    free(cwd);

    while (wsRoot.length() > 1 && wsRoot != "/")
    {
        std::ifstream ns3_file(wsRoot + "/ns3");
        std::ifstream cmake_file(wsRoot + "/CMakeLists.txt");
        std::ifstream version_file(wsRoot + "/VERSION");
        if ((ns3_file.good() || cmake_file.good()) && version_file.good())
        {
            return wsRoot + "/";
        }
        size_t lastSlash = wsRoot.find_last_of('/');
        if (lastSlash == std::string::npos || lastSlash == 0)
        {
            break;
        }
        wsRoot = wsRoot.substr(0, lastSlash);
    }
    char* cwd2 = getcwd(nullptr, 0);
    std::string fallback = cwd2 ? std::string(cwd2) + "/" : "./";
    free(cwd2);
    return fallback;
}


// std::string
// ToString (uint32_t value)
// {
//   std::stringstream ss;
//   ss << value;
//   return ss.str();
// }


void
CreateSingle2DPlotFile(std::string someImplementation, std::string onoff_traffic_mode, std::string transport_prot, std::string someUsedAlgorythm)  // for a single plot with N data-sets
{
  std::string wsRoot = GetWorkspaceRoot(); // absolute path with trailing /
  // .plt files live in GnuPlot_<mode>/ — no transport_prot subfolder here
  std::string gnuPlotFileLocation = wsRoot + "test_alphas_scripts/GnuPlot_" + onoff_traffic_mode + "/";

  std::string trace_parameter = someUsedAlgorythm + "_TestAccumulativeStatistics";
  std::string gnuPlotFileName = gnuPlotFileLocation + trace_parameter + "_" + someImplementation + ".plt";

  // Read the .plt file, inject transport_prot/ before onoff_traffic_mode/ in
  // every quoted path (both "set output" and "plot" data file references),
  // write a patched temp file, run gnuplot on it, then delete it.
  std::ifstream pltFile(gnuPlotFileName);
  if (!pltFile.is_open())
  {
    std::cerr << "Could not open .plt file: " << gnuPlotFileName << std::endl;
    return;
  }

  std::string patchedContent;
  std::string outputDir; // for mkdir -p
  const std::string needle = "/" + onoff_traffic_mode + "/";
  const std::string replacement = "/" + transport_prot + "/" + onoff_traffic_mode + "/";

  std::string line;
  while (std::getline(pltFile, line))
  {
    // Patch every occurrence of /onoff_traffic_mode/ inside quoted strings
    std::string patched = line;
    size_t pos = 0;
    while ((pos = patched.find(needle, pos)) != std::string::npos)
    {
      patched.replace(pos, needle.length(), replacement);
      pos += replacement.length();
    }

    // Track the output path so we can mkdir -p it
    if (patched.find("set output") != std::string::npos)
    {
      auto q1 = patched.find('"');
      auto q2 = patched.rfind('"');
      if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1)
      {
        std::string outputPath = patched.substr(q1 + 1, q2 - q1 - 1);
        if (outputPath.substr(0, 2) == "./")
          outputPath = wsRoot + outputPath.substr(2);
        auto lastSlash = outputPath.find_last_of('/');
        if (lastSlash != std::string::npos)
          outputDir = outputPath.substr(0, lastSlash);
      }
    }

    patchedContent += patched + "\n";
  }
  pltFile.close();

  if (!outputDir.empty())
    system(("mkdir -p \"" + outputDir + "\"").c_str());

  // Write patched content to a temp file next to the original
  std::string tmpPlotFileName = gnuPlotFileName + ".tmp.plt";
  std::ofstream tmpFile(tmpPlotFileName);
  if (!tmpFile.is_open())
  {
    std::cerr << "Could not write temp .plt file: " << tmpPlotFileName << std::endl;
    return;
  }
  tmpFile << patchedContent;
  tmpFile.close();

  // cd to workspace root so all relative paths inside the .plt resolve correctly
  system(("cd \"" + wsRoot + "\" && gnuplot \"" + tmpPlotFileName + "\"").c_str());

  // Clean up temp file
  std::remove(tmpPlotFileName.c_str());

}

// Function template to accept a reference to an array of any size
template <size_t N>
void
CreateMultiple2DPlotFiles(std::string someImplementation, std::string onoff_traffic_mode, std::string transport_prot, const std::string (&someUsedAlgorythmArray)[N])  // for a single plot with N data-sets
{
  for (size_t i = 0; i < N; i++)
  {
    CreateSingle2DPlotFile(someImplementation, onoff_traffic_mode, transport_prot, someUsedAlgorythmArray[i]);
  }
}

int main (int argc, char *argv[])
{ 
  std::string implementation = "MQueue_5ToS";  // "FIFO"/"MQueue_2ToS"/"MQueue_5ToS"
  std::string onOffTrafficMode = "Constant"; // "Constant"/"Uniform"/"Normal"
  std::string transportProt = "UDP"; // "UDP"/"TCP"
  // for a single plot: /////////////////////                                                  
  std::string usedAlgorythm = "DT_VaryingDValues"; 
  // Options: 
  // "DT"/"FB"/"All"
  // "FB_0.1"/"FB_0.2"/"FB_0.3"/"FB_0.4"/"FB_0.5"/"FB_0.6"/"FB_0.7"/"FB_0.8"/"FB_0.9"/"FB_All_D"/"FB_VaryingDValues"
  // "DT_0.1"/"DT_0.2"/"DT_0.3"/"DT_0.4"/"DT_0.5"/"DT_0.6"/"DT_0.7"/"DT_0.8"/"DT_0.9"/"DT_All_D"/"DT_VaryingDValues"
  // "DT_vs_FB_0.1"
  // CreateSingle2DPlotFile(implementation, onOffTrafficMode, transportProt, usedAlgorythm);
  ///////////////////////////////////////////
  
  // for multiple different plots:
  // std::string usedAlgorythmArray[] = {"DT_0.1","DT_0.2","DT_0.3","DT_0.4","DT_0.5","DT_0.6","DT_0.7","DT_0.8","DT_0.9","DT_All_D"};
  // std::string usedAlgorythmArray[] = {"FB_0.1","FB_0.2","FB_0.3","FB_0.4","FB_0.5","FB_0.6","FB_0.7","FB_0.8","FB_0.9","FB_All_D"};
  std::string usedAlgorythmArray[] = {"DT_vs_FB_0.1","DT_vs_FB_0.2","DT_vs_FB_0.3","DT_vs_FB_0.4","DT_vs_FB_0.5","DT_vs_FB_0.6","DT_vs_FB_0.7","DT_vs_FB_0.8","DT_vs_FB_0.9"};
  CreateMultiple2DPlotFiles(implementation, onOffTrafficMode, transportProt, usedAlgorythmArray);
}