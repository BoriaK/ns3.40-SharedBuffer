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
  std::string trafficControlType = "SharedBuffer_FB_v01"; // "SharedBuffer_DT_v01"/"SharedBuffer_FB_v01"/
  bool accumulateStats = false; // true/false. to acumulate run statistics in a single file
  std::string onOffTrafficMode = "Constant"; // "Constant"/"Uniform"/"Normal"
  // Run option:
  // (1) single d & alphas pair value
  // (2) single d, multiple alphas pairs
  // (3) mutiple d values, single alphas pair
  // (4) mutiple d values & multiple alphas pairs
  // (5) single/multiple D values. Alphas are determined by Predictive Model

  int runOption = 3; // [1, 2, 3, 4, 5]
  switch (runOption)
  {
    case 1: // single d & alphas pair value
    {
      // select a specific d value:
      double_t miceElephantProb = 0.3; // d (- [0.1, 0.9] the probability to generate mice compared to elephant packets.

      // select a specific alpha high/low pair value:
      double_t alphaHigh = 15;
      double_t alphaLow = 5;

      // viaFIFO(trafficControlType, onOffTrafficMode, miceElephantProb, alphaHigh, alphaLow, accumulateStats);
      // viaMQueues2ToS(trafficControlType, onOffTrafficMode, miceElephantProb, alphaHigh, alphaLow, accumulateStats);
      // viaMQueues5ToS(trafficControlType, onOffTrafficMode, miceElephantProb, alphaHigh, alphaLow, accumulateStats);
      viaMQueues5ToS_v2(trafficControlType, onOffTrafficMode, miceElephantProb, alphaHigh, alphaLow, accumulateStats);
      break;
    }
    case 2: // single d, multiple alphas pairs
    {
      // select a specific d value:
      double_t miceElephantProb = 0.3; // d (- [0.1, 0.9] the probability to generate mice compared to elephant packets.

      // run over an array of alphas high/low:
      // std::array<double_t, 21> alphaHigh_array = {20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0.5};
      // std::array<double_t, 21> alphaLow_array = {0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
      std::array<double_t, 31> alphaHigh_array = {30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0.5};
      std::array<double_t, 31> alphaLow_array = {0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};

      for (size_t i = 0; i < alphaHigh_array.size(); i++)
      {
        viaMQueues2ToS(trafficControlType, onOffTrafficMode, miceElephantProb, alphaHigh_array[i], alphaLow_array[i], accumulateStats);
        // viaMQueues5ToS(trafficControlType, onOffTrafficMode, miceElephantProb, alphaHigh_array[i], alphaLow_array[i], accumulateStats);
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
      bool adjustableAlphas = false;  // selects the optimal Alpha High/Low for each D value in VaryingD mode
      ////////////////////////////
      // select a specific alpha high/low value:
      double_t alphaHigh = 17;
      double_t alphaLow = 3;

      if (!VaryingD)
      {
        // Calculate the number of elements in the array
        std::size_t arrayLength = sizeof(miceElephantProb_array) / sizeof(miceElephantProb_array[0]);
        for (size_t i = 0; i < arrayLength; i++)  // iterate over all the d values in the array
        {
          // viaMQueues2ToS(trafficControlType, onOffTrafficMode, miceElephantProb_array[i], alphaHigh, alphaLow, accumulateStats);
          // viaMQueues5ToS(trafficControlType, onOffTrafficMode, miceElephantProb_array[i], alphaHigh, alphaLow, accumulateStats);
          viaMQueues5ToS_v2(trafficControlType, onOffTrafficMode, miceElephantProb_array[i], alphaHigh, alphaLow, accumulateStats);
        }
      }
      else // run all the d values in the array consecutivly in a single flow
      {
        // viaMQueues2ToSVaryingD(trafficControlType, onOffTrafficMode, miceElephantProb_array, alphaHigh, alphaLow, adjustableAlphas, accumulateStats);
        // viaMQueues5ToSVaryingD(trafficControlType, onOffTrafficMode, miceElephantProb_array, alphaHigh, alphaLow, adjustableAlphas, accumulateStats);
        viaMQueues5ToS_v2_VaryingD(trafficControlType, onOffTrafficMode, miceElephantProb_array, alphaHigh, alphaLow, adjustableAlphas, accumulateStats);
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
      std::array<double_t, 21> alphaHigh_array = {20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0.5};
      std::array<double_t, 21> alphaLow_array = {0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
      // std::array<double_t, 26> alphaHigh_array = {50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25};
      // std::array<double_t, 26> alphaLow_array = {0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};
      // std::array<double_t, 51> alphaHigh_array = {50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0.5};
      // std::array<double_t, 51> alphaLow_array = {0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50};
      // std::array<double_t, 101> alphaHigh_array = {100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0.5};
      // std::array<double_t, 101> alphaLow_array = {0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
      // std::array<double_t, 3> alphaHigh_array = {13, 15, 16};
      // std::array<double_t, 3> alphaLow_array = {7, 5, 4};

      if (!VaryingD)
      {
        // Calculate the number of elements in the array
        std::size_t arrayLength = sizeof(miceElephantProb_array) / sizeof(miceElephantProb_array[0]);
        for (size_t i = 0; i < arrayLength; i++)  // iterate over all the d values in the array
        {
          for (size_t j = 0; j < alphaHigh_array.size(); j++)
          {
            // viaMQueues2ToS(trafficControlType, onOffTrafficMode, miceElephantProb_array[i], alphaHigh_array[j], alphaLow_array[j], accumulateStats);
            // viaMQueues5ToS(trafficControlType, onOffTrafficMode, miceElephantProb_array[i], alphaHigh_array[j], alphaLow_array[j], accumulateStats);
            viaMQueues5ToS_v2(trafficControlType, onOffTrafficMode, miceElephantProb_array[i], alphaHigh_array[j], alphaLow_array[j], accumulateStats);
          }
        }
      }
      else
      {
        for (size_t i = 0; i < alphaHigh_array.size(); i++)
        {
          viaMQueues2ToSVaryingD(trafficControlType, onOffTrafficMode, miceElephantProb_array, alphaHigh_array[i], alphaLow_array[i], false, accumulateStats);
          viaMQueues5ToSVaryingD(trafficControlType, onOffTrafficMode, miceElephantProb_array, alphaHigh_array[i], alphaLow_array[i], false, accumulateStats);
        }
      }
      break;
    }
    case 5:  // single/multiple D values. Alphas are determined by Predictive Model
    {
      // select a specific d value:
      double_t miceElephantProb = 0.3; // d (- [0.1, 0.9] the probability to generate mice compared to elephant packets.
      viaMQueuesPredictive2ToS ("SharedBuffer_PredictiveFB_v01", onOffTrafficMode, miceElephantProb, accumulateStats);
    }
    default:
      break;
  }

  return 0;
}