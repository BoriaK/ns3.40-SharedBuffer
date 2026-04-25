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

#include "1In1Out_SharedBuffer_Scripts.cc"



// Only for Predictive Model
// "test_transient_handling" - tests transient behaviour of the Predictive Algorithm

int main ()
{
  std::string trafficControlType = "SharedBuffer_DT"; // "SharedBuffer_DT"/"SharedBuffer_FB"
  bool accumulateStats = true; // true/false. to acumulate run statistics in a single file
  std::string transportProt = "UDP"; // "UDP"/"TCP"
  std::string tcpType = "TcpTahoe"; // "TcpNewReno"/"TcpBbr"/"TcpTahoe"/"TcpNewRenoTest" - relevant for TCP only

  // for local d estimatiuon 
  double_t futurePossition = 0.5; // the possition of the estimation window in regards of past time samples/future samples.
  double_t winLength = 0.4; // estimation window length in time [sec], for transient handling.
  // start predictive model at: t0 - Tau*futurePossition
  double_t miceElephantProb = 0.2;
  // select a specific alpha high/low pair value:
  size_t alphaHigh = 15;
  size_t alphaLow = 5;
  // Transient parameters:
  double_t transientLength = 0.1; // {0.01, 0.02, 0.05, 0.1, 0.11, 0.12} [sec] length of the transient period.
  bool handleTransient = true; // true/false. to apply transient handling for the current flow or not. (relevant for Predictive model only)
  // Run option:
  // (1) single Low Priority stream. no transient is inserted
  // (2) a low priority stream reaches steady state. high priority transient is inserted.
  
  int runOption = 2; // [1, 2]
  switch (runOption)
  {
    case 1:  // no transient is added
    { 
      viaMQueuesPredictive1ToS (transportProt, tcpType, trafficControlType, miceElephantProb, alphaHigh, alphaLow, accumulateStats, futurePossition, winLength);
      break;
    }
    case 2:  // transient is added
    {
      viaMQueuesPredictive2ToS (transportProt, tcpType, trafficControlType, miceElephantProb, alphaHigh, alphaLow, futurePossition, winLength, transientLength, handleTransient, accumulateStats);
      break;
    }
    default:
      break;
  }

  return 0;
}