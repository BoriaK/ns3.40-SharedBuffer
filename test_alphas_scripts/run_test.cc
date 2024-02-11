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

#include <array>

#include "2In2Out_SharedBuffer_Scripts.cc"


int main ()
{ 
  std::string traffic_control_type = "SharedBuffer_FB_v01"; // "SharedBuffer_DT_v01"/"SharedBuffer_FB_v01" 
  bool accumulateStats = true; // true/false. to acumulate run statistics in a single file
  std::string onOffTrafficMode = "Normal"; // "Constant"/"Uniform"/"Normal" 
  
  int runOption = 4; // [1, 2, 3, 4]
  switch (runOption)
  {
    case 1: // single d & alphas pair value
    {
      // select a specific d value:
      double_t miceElephantProb = 0.5; // d (- [0.1, 0.9] the probability to generate mice compared to elephant packets.

      // select a specific alpha high/low pair value:
      double_t alpha_high = 17;
      double_t alpha_low = 3;

      // viaFIFO(traffic_control_type, onOffTrafficMode, miceElephantProb, alpha_high, alpha_low, accumulateStats);
      viaMQueues2ToS(traffic_control_type, onOffTrafficMode, miceElephantProb, alpha_high, alpha_low, accumulateStats);
      // viaMQueues5ToS(traffic_control_type, onOffTrafficMode, miceElephantProb, alpha_high, alpha_low, accumulateStats);
      break;
    }
    case 2: // single d, multiple alphas pairs
    {
      // select a specific d value:
      double_t miceElephantProb = 0.3; // d (- [0.1, 0.9] the probability to generate mice compared to elephant packets.

      // run over an array of alphas high/low:
      std::array<double_t, 21> alpha_high_array = {20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0.5};
      std::array<double_t, 21> alpha_low_array = {0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

      for (size_t i = 0; i < alpha_high_array.size(); i++)
      {
        viaMQueues2ToS(traffic_control_type, onOffTrafficMode, miceElephantProb, alpha_high_array[i], alpha_low_array[i], accumulateStats);
        // viaMQueues5ToS(traffic_control_type, onOffTrafficMode, miceElephantProb, alpha_high_array[i], alpha_low_array[i], accumulateStats);
      }
      break;
    }
    case 3: // mutiple d values, single alphas pair 
    {
      // could be loaded as individual values in a loop or as an array of consecutive D values in "VaryingD" mode
      // std::double_t miceElephantProb_array[] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9};
      std::double_t miceElephantProb_array[] = {0.2, 0.5, 0.8};
      ////////////////////////////
      bool VaryingD = true;
      bool adjustableAlphas = true;  // selects the optimal Alpha High/Low for each D value in VaryingD mode
      ////////////////////////////
      // select a specific alpha high/low value:
      double_t alpha_high = 17;
      double_t alpha_low = 3;

      if (!VaryingD)
      {
        // Calculate the number of elements in the array
        std::size_t arrayLength = sizeof(miceElephantProb_array) / sizeof(miceElephantProb_array[0]);
        for (size_t i = 0; i < arrayLength; i++)  // iterate over all the d values in the array 
        {
          // viaMQueues2ToS(traffic_control_type, onOffTrafficMode, miceElephantProb_array[i], alpha_high, alpha_low, accumulateStats);
          viaMQueues5ToS(traffic_control_type, onOffTrafficMode, miceElephantProb_array[i], alpha_high, alpha_low, accumulateStats);
        }
      }
      else // run all the d values in the array consecutivly in a single flow
      {
        // viaMQueues2ToSVaryingD(traffic_control_type, onOffTrafficMode, miceElephantProb_array, alpha_high, alpha_low, adjustableAlphas, accumulateStats);
        viaMQueues5ToSVaryingD(traffic_control_type, onOffTrafficMode, miceElephantProb_array, alpha_high, alpha_low, adjustableAlphas, accumulateStats);
      }
      break;
    }
    case 4: // multiple alphas pairs & mutiple d values
    {
      // could be loaded as individual values in a loop or as an array of consecutive D values in "VaryingD" mode
      std::double_t miceElephantProb_array[] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9};
      // std::double_t miceElephantProb_array[] = {0.2, 0.5, 0.8};

      ////////////////////////////
      bool VaryingD = false;
      // in this option adjustableAlphas is false. otherwise it would repeat the same iteration for each alpha high/low pair
      ////////////////////////////

      // run over an array of alphas high/low:
      std::array<double_t, 21> alpha_high_array = {20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0.5};
      std::array<double_t, 21> alpha_low_array = {0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
      // std::array<double_t, 3> alpha_high_array = {13, 15, 16};
      // std::array<double_t, 3> alpha_low_array = {7, 5, 4};

      if (!VaryingD)
      {
        // Calculate the number of elements in the array
        std::size_t arrayLength = sizeof(miceElephantProb_array) / sizeof(miceElephantProb_array[0]);
        for (size_t i = 0; i < arrayLength; i++)  // iterate over all the d values in the array 
        {
          for (size_t j = 0; j < alpha_high_array.size(); j++)
          {
            viaMQueues2ToS(traffic_control_type, onOffTrafficMode, miceElephantProb_array[i], alpha_high_array[j], alpha_low_array[j], accumulateStats);
            viaMQueues5ToS(traffic_control_type, onOffTrafficMode, miceElephantProb_array[i], alpha_high_array[j], alpha_low_array[j], accumulateStats);
          }
        }
      }
      else
      {
        for (size_t i = 0; i < alpha_high_array.size(); i++)
        {
          viaMQueues2ToSVaryingD(traffic_control_type, onOffTrafficMode, miceElephantProb_array, alpha_high_array[i], alpha_low_array[i], false, accumulateStats);
          viaMQueues5ToSVaryingD(traffic_control_type, onOffTrafficMode, miceElephantProb_array, alpha_high_array[i], alpha_low_array[i], false, accumulateStats);
        }
      }
      break;
    }
    default:
      break;
  }

  return 0;
}