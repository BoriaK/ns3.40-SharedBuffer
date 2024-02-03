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
  double_t miceElephantProb = 0.9; // d (- [0.1, 0.9] the probability to generate mice compared to elephant packets. 
  // in this simulation it effects silence time ratio for the onoff application
  ////////// for Varring D mode only /////////////
  std::array<double_t, 3> miceElephantProb_array = {0.2, 0.5, 0.7}; // 3 consequtive D values to use in a single flow
  bool adjustableAlphas = true;  // selects the optimal Alpha High/Low for each D value 
  ////////////////////////////////////////////////
  // std::string trafficDuration = "short"; // "short" = 2[sec] the total duration of OnOff traffic generation, 
  // or "long" = 3 segments of 2[sec] OnOff traffic generation, with 2[sec] duration of silence in between. 6[sec] total time
  bool accumulateStats = true; // true/false
  int runOption = 1; // [1, 2, 3]
  
  switch (runOption)
  {
    case 1:
    {
      // option 1: run a specific alpha high/low value:
      double_t alpha_high = 20;
      double_t alpha_low = 0.5;

      // viaMQueues5ToS(traffic_control_type, alpha_high, alpha_low, miceElephantProb, accumulateStats);
      // viaMQueues2ToS(traffic_control_type, alpha_high, alpha_low, miceElephantProb, accumulateStats);
      // viaMQueues5ToSVaryingD(traffic_control_type, alpha_high, alpha_low, miceElephantProb_array, adjustableAlphas, accumulateStats);
      viaMQueues2ToSVaryingD(traffic_control_type, alpha_high, alpha_low, miceElephantProb_array, adjustableAlphas, accumulateStats);
      break;
    }

    case 2:
    {
      // option 2: run over a specific array of alphas high/low:
      std::array<double_t, 21> alpha_high_array = {20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0.5};
      std::array<double_t, 21> alpha_low_array = {0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
      // std::array<double_t, 4> alpha_high_array = {20, 19, 18, 17};
      // std::array<double_t, 4> alpha_low_array = {0.5, 1, 2, 3};
      
      for (size_t i = 0; i < alpha_high_array.size(); i++)
      {
        // viaMQueues2ToS(traffic_control_type, alpha_high_array[i], alpha_low_array[i], miceElephantProb, accumulateStats);
        viaMQueues5ToS(traffic_control_type, alpha_high_array[i], alpha_low_array[i], miceElephantProb, accumulateStats);
      }
        break;
    }

    case 3:
    {
      // option 3: run over all alphas high/low in range:
      
      for (size_t a_h = 20; a_h > 0; a_h--)
      {
        // private case: a_l = 0.5
        double_t a_l = 0.5;
        viaMQueues2ToS(traffic_control_type, a_h, a_l, miceElephantProb, accumulateStats);
        for (size_t a_l = 1; a_l < 20; a_l++)
        {
          viaMQueues2ToS(traffic_control_type, a_h, a_l, miceElephantProb, accumulateStats);
        }
      }
      // private case: a_h = 0.5
      double_t a_h = 0.5;
      
      // private case: a_l = 0.5
      double_t a_l = 0.5;
      viaMQueues2ToS(traffic_control_type, a_h, a_l, miceElephantProb, accumulateStats);
      for (size_t a_l = 1; a_l < 20; a_l++)
      {
        viaMQueues2ToS(traffic_control_type, a_h, a_l, miceElephantProb, accumulateStats);
      }
    }

    default:
      break;
  }

  return 0;
}