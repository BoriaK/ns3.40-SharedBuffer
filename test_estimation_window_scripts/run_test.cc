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

// Only for Predictive Model
// "test_Estimation_Window" - tests "Window Length" and "Window Possition" for Predictive Algorythm 

int main ()
{
  std::string trafficControlType = "SharedBuffer_DT"; // "SharedBuffer_DT"/"SharedBuffer_FB"
  bool accumulateStats = true; // true/false. to acumulate run statistics in a single file
  std::string onOffTrafficMode = "Constant"; // "Constant"/"Uniform"/"Normal"

  // for local d estimatiuon 
  double_t tau = 0.05; // Tau is Estimation Window Length [Sec]. {0.03, 0.04, 0.05}
  double_t futurePossition = 0.75; // the placemant of the window in regards to t0 [fraction of Tau]. {0.25, 0.5, 0.75}
  // start predictive model at: t0 - Tau*futurePossition
  // the estimation window will be: [t0 - Tau*(1-futurePossition), t0 + Tau*futurePossition]

  // Run option:
  // (5) single D value. Alphas are determined by Predictive Model
  // (6) multiple D values. Alphas are determined by Predictive Model

  int runOption = 6; // [5, 6]
  switch (runOption)
  {
    case 5:  // single D value. Alphas are determined by Predictive Model
    {
      // this file is for debug, to trace the sequence of all estimated d values during the run
      // it needs to be erased before the simulation starts
      std::remove("Estimated_D_Values.dat");
      
      // select a specific d value:
      double_t miceElephantProb = 0.3; // d (- [0.1, 0.9] the probability to generate mice compared to elephant packets.
      // viaMQueuesPredictive2ToS ("SharedBuffer_PredictiveFB", onOffTrafficMode, miceElephantProb, accumulateStats); // not in use
      viaMQueuesPredictive5ToS_v2 (trafficControlType, onOffTrafficMode, miceElephantProb, accumulateStats, futurePossition, tau);
      break;
    }
    case 6:  // multiple D values. Alphas are determined by Predictive Model
    {
      // this file is for debug, to trace the sequence of all estimated d values during the run
      // it needs to be erased before the simulation starts
      std::remove("Estimated_D_Values.dat");

      // set an array of tested d values:
      std::double_t miceElephantProb_array[] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9};
      // Calculate the number of elements in the array
      std::size_t arrayLength = sizeof(miceElephantProb_array) / sizeof(miceElephantProb_array[0]);
      for (size_t i = 0; i < arrayLength; i++)  // iterate over all the d values in the array
      {
        viaMQueuesPredictive5ToS_v2 (trafficControlType, onOffTrafficMode, miceElephantProb_array[i], accumulateStats, futurePossition, tau);
      }
      break;
    }
    default:
      break;
  }

  return 0;
}