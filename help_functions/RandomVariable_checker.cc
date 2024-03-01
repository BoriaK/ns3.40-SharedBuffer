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


void
constantRandomVariableChecker(double_t rand_var)
{
  Ptr<ConstantRandomVariable> constantRandomVariable = ns3::CreateObject<ns3::ConstantRandomVariable>();
  constantRandomVariable->SetAttribute("Constant", DoubleValue(rand_var));

  double randConst = constantRandomVariable->GetConstant();
  double randVal = constantRandomVariable->GetValue();

  std::cout << "the constant returned by the RNG is: " << randConst << std::endl;
  std::cout << "the next random value returned by the RNG is: " << randVal << std::endl;
}

void
uniformRandomVariableChecker(double_t min_uni, double_t max_uni)
{
  RngSeedManager::SetSeed(12);
  // std::cout << "the RNG seed is: " << RngSeedManager::GetSeed() << std::endl;
  
  Ptr<UniformRandomVariable> uniformRandomVariable = ns3::CreateObject<ns3::UniformRandomVariable>();
  uniformRandomVariable->SetAttribute("Min", DoubleValue(min_uni));
  uniformRandomVariable->SetAttribute("Max", DoubleValue(max_uni));

  // RngSeedManager::SetRun(1);
  // std::cout << "the run number is: " << RngSeedManager::GetRun() << std::endl;

  double uniVar0, uniVar1, uniVar2;
  for (size_t i = 0; i < 3; i++)
  { 
    uniformRandomVariable->SetStream(5);
    // std::cout << "the stream number for the RngStream " << uniformRandomVariable->GetStream() << std::endl;
    
    uniVar0 = uniformRandomVariable->GetValue();
    uniVar1 = uniformRandomVariable->GetValue();
    uniVar2 = uniformRandomVariable->GetValue();

    std::cout << "the numbers drawn from the Uniform distrebution returned by the RNG are: " << uniVar0 << " ,"
                                                                                             << uniVar1 << " ,"
                                                                                             << uniVar2 << std::endl;
  }
}

void
normalRandomVariableChecker(double_t mean_norm, double_t variance_norm)
{
  RngSeedManager::SetSeed(12);
  // std::cout << "the RNG seed is: " << RngSeedManager::GetSeed() << std::endl;
  RngSeedManager::SetRun(1);
  // std::cout << "the run number is: " << RngSeedManager::GetRun() << std::endl;

  Ptr<NormalRandomVariable> normalRandomVariable = ns3::CreateObject<ns3::NormalRandomVariable>();
  normalRandomVariable->SetAttribute("Mean", DoubleValue(mean_norm));
  normalRandomVariable->SetAttribute("Variance", DoubleValue(variance_norm));

  double normVar0, normVar1, normVar2;
  for (size_t i = 0; i < 3; i++)
  { 
    normalRandomVariable->SetStream(5);
    // std::cout << "the stream number for the RngStream " << normalRandomVariable->GetStream() << std::endl;

    normVar0 = normalRandomVariable->GetValue();
    normVar1 = normalRandomVariable->GetValue();
    normVar2 = normalRandomVariable->GetValue();

    std::cout << "the numbers drawn from the Normal distrebution returned by the RNG are: " << normVar0 << " ,"
                                                                                            << normVar1 << " ,"
                                                                                            << normVar2 << std::endl;
  }
}

// void
// poissonRandomVariableChecker(double_t mean_norm, double_t variance_norm)
// {
//   // RngSeedManager::SetSeed(12);
//   // std::cout << "the RNG seed is: " << RngSeedManager::GetSeed() << std::endl;
//   // RngSeedManager::SetRun(1);
//   // std::cout << "the run number is: " << RngSeedManager::GetRun() << std::endl;

//   Ptr<NormalRandomVariable> normalRandomVariable = ns3::CreateObject<ns3::NormalRandomVariable>();
//   normalRandomVariable->SetAttribute("Mean", DoubleValue(mean_norm));
//   normalRandomVariable->SetAttribute("Variance", DoubleValue(variance_norm));

//   double normVar0, normVar1, normVar2;
//   for (size_t i = 0; i < 3; i++)
//   { 
//     normalRandomVariable->SetStream(5);
//     // std::cout << "the stream number for the RngStream " << normalRandomVariable->GetStream() << std::endl;

//     normVar0 = normalRandomVariable->GetValue();
//     normVar1 = normalRandomVariable->GetValue();
//     normVar2 = normalRandomVariable->GetValue();

//     std::cout << "the numbers drawn from the Normal distrebution returned by the RNG are: " << normVar0 << " ,"
//                                                                                             << normVar1 << " ,"
//                                                                                             << normVar2 << std::endl;
//   }
// }

int main (int argc, char *argv[])
{ 
  // constantRandomVariableChecker(0.5);
  // uniformRandomVariableChecker(0, 1);
  normalRandomVariableChecker(1, 0.5);
}