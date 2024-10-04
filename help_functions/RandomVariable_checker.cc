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

#include "ns3/gnuplot.h"
#include "ns3/histogram.h"


using namespace ns3;


/**
 * Round a double number to the given precision.
 * For example, `dround(0.234, 0.1) = 0.2`
 * and `dround(0.257, 0.1) = 0.3`
 * \param [in] number The number to round.
 * \param [in] precision The least significant digit to keep in the rounding.
 * \returns \pname{number} rounded to \pname{precision}.
 */
double
dround(double number, double precision)
{
    number /= precision;
    if (number >= 0)
    {
        number = std::floor(number + 0.5);
    }
    else
    {
        number = std::ceil(number - 0.5);
    }
    number *= precision;
    return number;
}

/**
 * Generate a histogram from an array of doubles.
 * \param [in] in_array The RandomVariableStream to sample.
 * \param [in] precision The precision to round samples to.
 * \param [in] title The title for the histogram.
 * \param [in] impulses Set the plot style to IMPULSES.
 * \return The histogram as a GnuPlot data set.
 */
GnuplotDataset
createHistogram(const std::vector<double>& in_array,
          double precision,
          const std::string& title,
          bool impulses = false)
{
    typedef std::map<double, unsigned int> histogram_maptype;
    histogram_maptype histogram;

    for (double val : in_array)
    {
        double rounded_val = dround(val, precision);
        ++histogram[rounded_val];
    }

    Gnuplot2dDataset data;
    data.SetTitle(title);

    if (impulses)
    {
        data.SetStyle(Gnuplot2dDataset::IMPULSES);
    }

    for (auto hi = histogram.begin(); hi != histogram.end(); ++hi)
    {
        data.Add(hi->first, (double)hi->second / (double)in_array.size() / precision);
    }

    return data;
}


void
createAndPlotHistogram(std::string destrebution, const std::vector<double>& in_array, double precision)
{
  std::string plotFileName = destrebution;
  Gnuplot plot(plotFileName+".png");
  plot.SetTitle("NormalRandomVariable");
  // Make the graphics file, which the plot file will create when it
  // is used with Gnuplot, be a PNG file.
  plot.SetTerminal("png");
  plot.AddDataset(createHistogram(in_array, precision, destrebution, false));
  
  // Open the plot file.
  std::ofstream plotFile(plotFileName + ".plt");
  // Write the plot file.
  plot.GenerateOutput(plotFile);
  // Close the plot file.
  plotFile.close();
  // command line needs to be in ./ns-3-dev-git$ inorder for the script to produce gnuplot correctly///
  system (("gnuplot " + plotFileName + ".plt").c_str ());
  
  std::remove("NormalRandomVariableHistogram.plt");
  std::cout << "" << std::endl;  
}


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
  std::cout << "the RNG seed is: " << RngSeedManager::GetSeed() << std::endl;
  RngSeedManager::SetRun(1);
  std::cout << "the run number is: " << RngSeedManager::GetRun() << std::endl;
  
  Ptr<UniformRandomVariable> uniformRandomVariable = ns3::CreateObject<ns3::UniformRandomVariable>();
  uniformRandomVariable->SetAttribute("Min", DoubleValue(min_uni));
  uniformRandomVariable->SetAttribute("Max", DoubleValue(max_uni));


  /////// to check that the RNG produces the exact same values for each instance: //////
  /////// generate an array of random numbers of the set distrebution             //////
  /////// repeat 3 times, and see that each time the RNG peoduces the same array  //////
  int vacLength = 5;
  for (size_t i = 0; i < 3; i++)
  { 
    uniformRandomVariable->SetStream(2 + i);
    std::cout << "the stream number for the RngStream " << uniformRandomVariable->GetStream() << std::endl;
    
    std::cout << "the numbers drawn from the Uniform distrebution returned by the RNG are: " << std::endl;
    for (size_t j = 0; j < vacLength; j++)
    { 
      std::cout << uniformRandomVariable->GetValue() << ", ";
    }
    std::cout << std::endl;
  }


  /////// plot Histogram and save it to .png //////
  // std::vector<double_t> uniformVarVector;
  // int arrLength = 10000;
  // // std::cout << "the numbers drawn from the Normal distrebution returned by the RNG are: " << std::endl;
  // for (size_t i = 0; i < arrLength; i++)
  // { 
  //   uniformVarVector.push_back(uniformRandomVariable->GetValue());
  //   // std::cout << uniformVarVector[i] << ", ";
  // }
  // // std::cout << std::endl;
  
  // createAndPlotHistogram("UniformDestrebution",uniformVarVector, 0.1);

}

void
normalRandomVariableChecker(double_t mean_norm, double_t variance_norm)
{
  RngSeedManager::SetSeed(12);
  std::cout << "the RNG seed is: " << RngSeedManager::GetSeed() << std::endl;
  RngSeedManager::SetRun(1);
  std::cout << "the run number is: " << RngSeedManager::GetRun() << std::endl;

  Ptr<NormalRandomVariable> normalRandomVariable = ns3::CreateObject<ns3::NormalRandomVariable>();
  normalRandomVariable->SetAttribute("Mean", DoubleValue(mean_norm));
  normalRandomVariable->SetAttribute("Variance", DoubleValue(variance_norm));

  // normalRandomVariable->SetStream(2);
  // std::cout << "the stream number for the RngStream " << normalRandomVariable->GetStream() << std::endl;

  // to make sure that the RNG generates the same squence of random numbers each iteration:
  // make sure that the RNG STREAM is set to the same value.
  // to make sure that the RNG generates different sequences of random numbers each itteration:
  // make sure that the RNG STREAM is NOT set to the same number.
  int vecLength = 5;
  for (size_t i = 0; i < 3; i++)
  { 
    normalRandomVariable->SetStream(2 + i);
    std::cout << "the stream number for the RngStream " << normalRandomVariable->GetStream() << std::endl;
    
    std::cout << "the numbers drawn from the Normal distribution returned by the RNG are: " << std::endl;
    for (size_t j = 0; j < vecLength; j++)
    { 
      std::cout << normalRandomVariable->GetValue() << ", ";
    }
    std::cout << std::endl;
  }
  
  /////// plot Histogram and save it to .png //////
  // std::vector<double_t> normVarVector;
  // int arrLength = 10000;
  // // std::cout << "the numbers drawn from the Normal distrebution returned by the RNG are: " << std::endl;
  // for (size_t i = 0; i < arrLength; i++)
  // { 
  //   normVarVector.push_back(normalRandomVariable->GetValue());
  //   // std::cout << normVarVector[i] << ", ";
  // }
  // std::cout << std::endl;
  
  // createAndPlotHistogram("NormalDestrebution",normVarVector,0.1);                                                                                   
}

void
exponentialRandomVariableChecker(double_t mean_exp, double_t bound_exp)
{
  RngSeedManager::SetSeed(12);
  std::cout << "the RNG seed is: " << RngSeedManager::GetSeed() << std::endl;
  RngSeedManager::SetRun(1);
  std::cout << "the run number is: " << RngSeedManager::GetRun() << std::endl;

  Ptr<ExponentialRandomVariable> exponentialRandomVariable = ns3::CreateObject<ns3::ExponentialRandomVariable>();
  exponentialRandomVariable->SetAttribute("Mean", DoubleValue(mean_exp));
  exponentialRandomVariable->SetAttribute("Bound", DoubleValue(bound_exp));

  /////// to check that the RNG produces the exact same values for each instance: //////
  /////// generate an array of random numbers of the set distrebution             //////
  /////// repeat 3 times, and see that each time the RNG peoduces the same array  //////
  int vacLength = 5;
  for (size_t i = 0; i < 3; i++)
  {  
    exponentialRandomVariable->SetStream(2 + i);
    std::cout << "the stream number for the RngStream " << exponentialRandomVariable->GetStream() << std::endl;

    std::cout << "the numbers drawn from the exponential distrebution returned by the RNG are: " << std::endl;
    for (size_t j = 0; j < vacLength; j++)
    { 
      std::cout << exponentialRandomVariable->GetValue() << ", ";
    }
    std::cout << std::endl;
  }
  
  /////// plot Histogram and save it to .png //////
  // std::vector<double_t> exponentialVarVector;
  // int arrLength = 10000;
  // // std::cout << "the numbers drawn from the Normal distrebution returned by the RNG are: " << std::endl;
  // for (size_t i = 0; i < arrLength; i++)
  // { 
  //   exponentialVarVector.push_back(exponentialRandomVariable->GetValue());
  // }
  // createAndPlotHistogram("ExponentialDestrebution",exponentialVarVector,0.1);                                                                                   
}

int main (int argc, char *argv[])
{ 
  // constantRandomVariableChecker(0.5);  // input parameter is the constant random variable
  // uniformRandomVariableChecker(0, 1); // input parameters are: lower bound, upper bound
  // normalRandomVariableChecker(1, 0.5); // input parameters are: mean, variance
  exponentialRandomVariableChecker(1,1000); // input parameters are: mean, upper bound
}