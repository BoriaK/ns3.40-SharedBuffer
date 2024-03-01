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
#include "ns3/random-variable-stream.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"


using namespace ns3;

double_t
CalculateMiceOffTime (double_t traffic_gen_duration, double_t mice_on_time, uint32_t data_rate, double_t mice_elephant_prob, uint32_t total_num_of_packets, double_t hp_lp_prop)
{
  double_t off_time = 0;
  int32_t miceElephantProbIndex = 10 * mice_elephant_prob;
  double_t correctionFactor = 0;
  switch (miceElephantProbIndex)
  {
  case 1:
    correctionFactor = 0.15;
    break;
  case 2:
    correctionFactor = 0.07;
    break;
  case 3:
    correctionFactor = 0.05;
    break;
  case 4:
    correctionFactor = 0.03;
    break;    
  case 5:
    correctionFactor = 0.02;
    break;
  case 6:
    correctionFactor = 0.018;
    break;
  case 7:
    correctionFactor = 0.013;
    break;
  case 8:
    correctionFactor = 0.012;
    break;

  default:
    break;
  }
  
  off_time = ((traffic_gen_duration * mice_on_time * data_rate) / (8 * 1024 * mice_elephant_prob * total_num_of_packets)) - mice_on_time + correctionFactor;
  
  return off_time;
}

double_t
CalculateElephantOffTime (double_t traffic_gen_duration, double_t elephant_on_time, uint32_t data_rate, double_t mice_elephant_prob, uint32_t total_num_of_packets, double_t hp_lp_prop)
{
  double_t off_time = 0;
  int32_t miceElephantProbIndex = 10 * mice_elephant_prob;
  double_t correctionFactor = 0;
  switch (miceElephantProbIndex)
  {
  case 1:
    correctionFactor = 0.16;
    break;
  case 2:
    correctionFactor = 0.17;
    break;
  case 3:
    correctionFactor = 0.2;
    break;
  case 4:
    correctionFactor = 0.2;
    break;    
  case 5:
    correctionFactor = 0.2;
    break;
  case 6:
    correctionFactor = 0.25;
    break;
  case 7:
    correctionFactor = 0.3;
    break;
  case 8:
    correctionFactor = 0.45;
    break;

  default:
    break;
  }
  
  off_time = ((traffic_gen_duration * elephant_on_time * data_rate) / (8 * 1024 * (1 - mice_elephant_prob) * total_num_of_packets)) - elephant_on_time + correctionFactor;
  return off_time;
}

void
offTimeCalculatorChecker(double_t traffic_gen_duration, double_t mice_on_time, double_t elephant_on_time, double_t data_rate, double_t mice_elephant_prob, double_t total_num_of_packets, double_t hp_lp_prop)
{
  double_t miceOffTime = CalculateMiceOffTime(traffic_gen_duration, mice_on_time, data_rate, mice_elephant_prob, total_num_of_packets, hp_lp_prop); // [sec]
  double_t elephantOffOffTime = CalculateElephantOffTime(traffic_gen_duration, elephant_on_time, data_rate, mice_elephant_prob, total_num_of_packets, hp_lp_prop); // [sec]

  std::cout << "for d: " << mice_elephant_prob << " High priority packets Off time is: " << miceOffTime << std::endl;
  std::cout << "for d: " << mice_elephant_prob << " Low priority packets Off time is: " << elephantOffOffTime << std::endl;
}


int main (int argc, char *argv[])
{ 
  double_t trafficGenDuration = 2; // [sec]
  double_t miceOnTime = 0.05; // [sec]
  double_t elephantOnTime = 0.5; // [sec]
  uint32_t dataRate = 2e6; // [bps] 
  int32_t numOfTotalPackets = 795; // [packets]
  double_t propHpLp = 1; // proportion between High/Low Priority total OnOff applications 

  double_t miceElephantProb = 0.5;
  offTimeCalculatorChecker(trafficGenDuration, miceOnTime, elephantOnTime, dataRate, miceElephantProb, numOfTotalPackets, propHpLp);
  
  // std::array<double_t, 8> miceElephantProbArray = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8}; 
  // for (size_t i = 0; i < 8; i++)
  // {
  //   offTimeCalculatorChecker(trafficGenDuration, miceOnTime, elephantOnTime, dataRate, miceElephantProbArray[i], numOfTotalPackets, propHpLp);
  // }
}