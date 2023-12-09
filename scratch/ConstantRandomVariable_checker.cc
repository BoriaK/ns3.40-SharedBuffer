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
#include <stdint.h>

#include "ns3/core-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"


using namespace ns3;



int main (int argc, char *argv[])
{ 
  Ptr<ConstantRandomVariable> constantRandomVariable = ns3::CreateObject<ns3::ConstantRandomVariable>();
  constantRandomVariable->SetAttribute("Constant", DoubleValue(0.5));

  double randConst = constantRandomVariable->GetConstant();
  double randVal = constantRandomVariable->GetValue();

  std::cout << "the constant returned by the RNG is: " << randConst << std::endl;
  std::cout << "the next random value returned by the RNG is: " << randVal << std::endl;
}