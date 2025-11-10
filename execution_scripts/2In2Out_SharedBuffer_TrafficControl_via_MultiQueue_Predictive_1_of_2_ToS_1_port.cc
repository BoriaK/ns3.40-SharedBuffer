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
/*
 * Basic Topology: 
 * 1 client server: t0, connected to Rx port: p0, on a switch: S,
 * and Tx port: p0, connected to 1 receiver: r0.
 * this design is based on DumbellTopoplogy model.
 * in this version all the NetDevices are created first and the TrafficControllHelper is installed on them simultanoiusly
 *
 *     t0--p0-[S]-P0--r0
 * 
 * 
 * Additional "model" at t-T/2 to predict traffic:
 *     t0--p0-[S]-P0--r0
 * 
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <unistd.h>  // for getcwd
#include <cstdlib>   // for free
#include <string>
#include <list>
#include <array>
#include <filesystem>
#include <map>

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tutorial-app.h"
#include "ns3/custom_onoff-application.h"
#include "ns3/names.h"
#include "ns3/stats-module.h"

// There are 2 servers connecting to a leaf switch
#define SERVER_COUNT 1
#define SWITCH_COUNT 1
#define RECIEVER_COUNT 1

#define SWITCH_RECIEVER_CAPACITY 500*1e3        // Leaf-Spine Capacity 500Kbps/queue/port
#define SERVER_SWITCH_CAPACITY 5*1e6          // Total Serever-Leaf Capacity 5Mbps/queue/port
#define LINK_LATENCY MicroSeconds(20)           // each link latency 10 MicroSeconds 
#define BUFFER_SIZE 250                         // Shared Buffer Size for a single queue per port. 250 Packets

// The simulation starting and ending time
#define START_TIME 0.0
#define DURATION_TIME 2
#define END_TIME 60

// The flow port range, each flow will be assigned a random port number within this range
#define SERV_PORT_P1 50001

#define PACKET_SIZE 1024 // 1024 [bytes]

using namespace ns3;

// Function to get workspace root directory
std::string GetWorkspaceRoot() {
    // Try to get current working directory
    char* cwd = getcwd(nullptr, 0);
    if (cwd == nullptr) {
        // Fallback to relative path if getcwd fails
        return "./Trace_Plots/1In1Out/";
    }
    
    std::string workspaceDir(cwd);
    free(cwd);
    
    // Find the workspace root by looking for workspace identifier files
    std::string wsRoot = workspaceDir;
    
    // Keep going up directories until we find ns3 workspace markers
    while (wsRoot != "/" && wsRoot.length() > 1) {
        // Check for ns3 workspace indicators
        std::ifstream ns3_file(wsRoot + "/ns3");
        std::ifstream cmake_file(wsRoot + "/CMakeLists.txt");
        std::ifstream version_file(wsRoot + "/VERSION");
        
        if ((ns3_file.good() || cmake_file.good()) && version_file.good()) {
            // Found workspace root
            return wsRoot + "/Trace_Plots/1In1Out/";
        }
        
        // Go up one directory level
        size_t lastSlash = wsRoot.find_last_of('/');
        if (lastSlash == std::string::npos || lastSlash == 0) {
            break;
        }
        wsRoot = wsRoot.substr(0, lastSlash);
    }
    
    // If we couldn't find the workspace root, use current directory
    return workspaceDir + "/Trace_Plots/1In1Out/";
}

std::string root = GetWorkspaceRoot();
// std::string datDir = root + "/Trace_Plots/1In1Out/Predictive/";
std::string datDir = root + "/Predictive/";
std::string traffic_control_type = "SharedBuffer_DT"; // "SharedBuffer_DT"/"SharedBuffer_FB"
std::string implementation = "via_MultiQueues/2_ToS";  // "via_NetDevices"/"via_FIFO_QueueDiscs"/"via_MultiQueues"
std::string usedAlgorythm;  // "PredictiveDT"/"PredictiveFB"
std::string onOffTrafficMode = "Constant"; // "Constant"/"Uniform"/"Normal"
// // per-run output directory (set in main)
// std::string runDir = "";
// for OnOff Aplications
uint32_t dataRate = 2; // [Mbps] data generation rate for a single OnOff application
// time interval values
double_t trafficGenDuration = DURATION_TIME; // [sec] initilize for a single OnOff segment

uint32_t prev = 0;
Time prevTime = Seconds (0);

NS_LOG_COMPONENT_DEFINE ("1In1Out");

std::string
NDevicePointerToString (Ptr<NetDevice> ndevpointer)
{
  std::stringstream ss;
  ss << ndevpointer;
  return ss.str();
}

std::string
ToString (uint32_t value)
{
  std::stringstream ss;
  ss << value;
  return ss.str();
}

std::string
IntToString (u_int32_t value)
{
  std::stringstream ss;
  ss << value;
  return ss.str();
}

std::string
DoubleToString (double_t value)
{
  std::stringstream ss;
  ss << value;
  return ss.str();
}

std::string
StringCombine (std::string A, std::string B, std::string C)
{
  std::stringstream ss;
  ss << A << B << C;
  return ss.str();
}
// functions to monitor Shared Buffer packets on Traffic Control Layer
void
TrafficControlPacketsInSharedQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream tcpisq (datDir + "/TrafficControlPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  tcpisq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tcpisq.close ();
  
  // std::cout << "PacketsInSharedBuffer: " << newValue << std::endl;
}

void
TrafficControlHighPriorityPacketsInSharedQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream tchppisq (datDir + "/TrafficControlHighPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  tchppisq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tchppisq.close ();
  
  // std::cout << "HighPriorityPacketsInSharedBuffer: " << newValue << std::endl;
}

void
TrafficControlLowPriorityPacketsInSharedQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream tclppisq (datDir + "/TrafficControlLowPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  tclppisq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tclppisq.close ();
  
  // std::cout << "LowPriorityPacketsInSharedBuffer: " << newValue << std::endl;
}

// Trace the Threshold Value for High Priority packets in the Shared Queue
void
TrafficControlThresholdHighTrace (size_t index, float_t oldValue, float_t newValue)  // added by me, to monitor Threshold
{
  std::ofstream tchpthr (datDir + "/TrafficControlHighPriorityQueueThreshold_" + ToString(index) + ".dat", std::ios::out | std::ios::app);
  tchpthr << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tchpthr.close ();

  // std::cout << "HighPriorityQueueThreshold on port: " << index << " is: " << newValue << " packets " << std::endl;
}

// Trace the Threshold Value for Low Priority packets in the Shared Queue
void
TrafficControlThresholdLowTrace (size_t index, float_t oldValue, float_t newValue)  // added by me, to monitor Threshold
{
  std::ofstream tclpthr (datDir + "/TrafficControlLowPriorityQueueThreshold_" + ToString(index) + ".dat", std::ios::out | std::ios::app);
  tclpthr << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tclpthr.close ();
  
  std::cout << "LowPriorityQueueThreshold on port: " << index << " is: " << newValue << " packets " << std::endl;
}

//for MQueues
void
MultiQueueDiscPacketsInQueueTrace (size_t portIndex, size_t queueIndex, uint32_t oldValue, uint32_t newValue)
{
  std::ofstream qdpiq (datDir + "/port_" + ToString(portIndex) + "_queue_" + ToString(queueIndex) + "_PacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  qdpiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  qdpiq.close ();
  
  std::cout << "QueueDiscPacketsInPort " << portIndex << " Queue " << queueIndex << ": " << newValue << std::endl;
}

// for debug:
// void
// HighPriorityMultiQueueDiscPacketsInQueueTrace (size_t portIndex, size_t queueIndex, uint32_t oldValue, uint32_t newValue)
// {
//   std::ofstream hpqdpiq (datDir + "/port_" + ToString(portIndex) + "_queue_" + ToString(queueIndex) + "_HighPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
//   hpqdpiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
//   hpqdpiq.close ();
  
//   std::cout << "HighPriorityQueueDiscPacketsInPort " << portIndex << " Queue " << queueIndex << ": " << newValue << std::endl;
// }
//////////////////////////////////////////////////////

// void
// LowPriorityMultiQueueDiscPacketsInQueueTrace (size_t portIndex, size_t queueIndex, uint32_t oldValue, uint32_t newValue)
// {
//   std::ofstream lpqdpiq (datDir + "/port_" + ToString(portIndex) + "_queue_" + ToString(queueIndex) + "_LowPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
//   lpqdpiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
//   lpqdpiq.close ();
  
//   std::cout << "LowPriorityQueueDiscPacketsInPort " << portIndex << " Queue " << queueIndex << ": " << newValue << std::endl;
// }
/////////////////
// for FIFO
void
QueueDiscPacketsInQueueTrace (size_t index, uint32_t oldValue, uint32_t newValue)
{
  std::ofstream qdpiq (datDir+ "/queueDisc_" + ToString(index) + "_PacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  qdpiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  qdpiq.close ();
  
  std::cout << "QueueDiscPacketsInQueue " << index << ": " << newValue << std::endl;
}

void
HighPriorityQueueDiscPacketsInQueueTrace (size_t index, uint32_t oldValue, uint32_t newValue)
{
  std::ofstream hpqdpiq (datDir + "/queueDisc_" + ToString(index) + "_HighPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  hpqdpiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  hpqdpiq.close ();
  
  std::cout << "HighPriorityQueueDiscPacketsInQueue " << index << ": " << newValue << std::endl;
}

void
LowPriorityQueueDiscPacketsInQueueTrace (size_t index, uint32_t oldValue, uint32_t newValue)
{
  std::ofstream lpqdpiq (datDir + "/queueDisc_" + ToString(index) + "_LowPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  lpqdpiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  lpqdpiq.close ();
  
  std::cout << "LowPriorityQueueDiscPacketsInQueue " << index << ": " << newValue << std::endl;
}

// to monitor the OnOff state of a single OnOff Application in real time
void
checkOnOffState(Ptr<Application>& app, std::string predictive_status, size_t portIndex, size_t appIndex, double maxDuration, double startTime)
{
    double elapsedTime = Simulator::Now().GetSeconds() - startTime;

    // Stop monitoring if the elapsed time exceeds maxDuration
    if (elapsedTime > maxDuration) 
    {
        return; // Stop the monitoring
    }
    
    // Attempt to cast to PrioOnOffApplication
    Ptr<PrioOnOffApplication> prioOnOffApp = DynamicCast<PrioOnOffApplication>(app);
    bool appState = prioOnOffApp->GetCurrentState();

    // Check OnOff state every 1/100 of a second
    Simulator::Schedule(Seconds(0.001), &checkOnOffState, app, predictive_status, portIndex, appIndex, maxDuration, startTime);

    // save to .dat file
    std::ofstream fPlotOnOffState(std::stringstream(datDir + 
    "/" + predictive_status + "_port_" + IntToString(portIndex) + "_app_" + IntToString(appIndex) + "_OnOffStateTrace.dat").str(),
                             std::ios::out | std::ios::app);

    fPlotOnOffState << Simulator::Now().GetSeconds() << " " << appState << std::endl;
    fPlotOnOffState.close();
}

// to calculate the OnOff traffic generated by all OnOff applications in real time
void
returnOnOffTraffic(const ApplicationContainer& appContainer, double maxDuration, double startTime)
{
    double elapsedTime = Simulator::Now().GetSeconds() - startTime;

    // Stop monitoring if the elapsed time exceeds maxDuration
    if (elapsedTime > maxDuration) {
        return; // Stop the monitoring
    }
    
    int traffic = 0;
    for (size_t i = 0; i < appContainer.GetN(); i++)
    {
      Ptr<Application> app = appContainer.Get(i);
      // Attempt to cast to PrioOnOffApplication
      Ptr<PrioOnOffApplication> prioOnOffApp = DynamicCast<PrioOnOffApplication>(app);
      if (prioOnOffApp) 
      { // Only proceed if the cast is successful
            bool appState = prioOnOffApp->GetCurrentState();
            traffic += appState ? 1 : 0; // Count only if the app is in the "on" state
      }
    }

    // Check OnOff state every 1/1000 of a second
    Simulator::Schedule(Seconds(0.0001), &returnOnOffTraffic, appContainer, maxDuration, startTime);
    
    // for testing:
    // std::cout <<Simulator::Now ().GetSeconds () << " OnOff Traffic: " << traffic << std::endl;

    // save to .dat file
    std::ofstream fPlotOnOffTrafficState(std::stringstream(datDir + "/generatedOnOffTrafficTrace.dat").str(),
                             std::ios::out | std::ios::app);
    fPlotOnOffTrafficState << Simulator::Now().GetSeconds() << " " << traffic << std::endl;
    fPlotOnOffTrafficState.close();
}

void
generate1DGnuPlotFromDatFile(std::string datFileName)
{
  std::string plotFileName = datFileName.substr(0, datFileName.length() - 4);
  Gnuplot plot(plotFileName + ".png");
  plot.SetTitle("generated On Off Traffic[packets] vs Time[sec]");
  // Make the graphics file, which the plot file will create when it
  // is used with Gnuplot, be a PNG file.
  plot.SetTerminal("png");
  // Set the labels for each axis. xlabel/ylabel
  plot.SetLegend("Time[sec]", "Traffic");
  
  Gnuplot2dDataset dataset;
  dataset.SetTitle("generated Packets");
  dataset.SetStyle(Gnuplot2dDataset::STEPS);
  
  // load a dat file as data set for plotting
  std::ifstream file(datFileName);
  double x, y;
  x = 0;
  y = 0;

  while (file >> x >> y) 
  {
    dataset.Add(x, y);
  }
  // Add the dataset to the plot.
  plot.AddDataset(dataset);
  
  // Open the plot file.
  std::ofstream plotFile(plotFileName + ".plt");
  // Write the plot file.
  plot.GenerateOutput(plotFile);
  // Close the plot file.
  plotFile.close();
  // command line needs to be in ./ns-3-dev-git$ inorder for the script to produce gnuplot correctly///
  system (("gnuplot " + plotFileName + ".plt").c_str ());
}

void
generate2DGnuPlotFromDatFile(std::string datFileName, std::string priority)
{
  std::string location = datFileName.substr(0, datFileName.length() - 38);
  std::string plotFileName = datFileName.substr(0, datFileName.length() - 23) + priority;
  std::string portInd = datFileName.substr(datFileName.length() - 33, 1);
  std::string queueInd = datFileName.substr(datFileName.length() - 25, 1);
  Gnuplot plot(plotFileName + ".png");
  std::string plotTitle = "port " + portInd + " queue " + queueInd + " " + priority + " Priority Packets vs Threshold";
  plot.SetTitle(plotTitle);
  // Make the graphics file, which the plot file will create when it
  // is used with Gnuplot, be a PNG file.
  plot.SetTerminal("png");
  // Set the labels for each axis. xlabel/ylabel
  plot.SetLegend("Time[sec]", "PacketsInQueue");
  
  // add the desired trace parameters to plot
  Gnuplot2dDataset dataset1, dataset2;

  dataset1.SetTitle("generated Packets");
  dataset1.SetStyle(Gnuplot2dDataset::LINES);
  
  // load a dat file as data set for plotting
  std::ifstream file1(datFileName);
  double x, y;
  x = 0;
  y = 0;

  while (file1 >> x >> y) 
  {
    dataset1.Add(x, y);
  }
  // Add the dataset to the plot.
  plot.AddDataset(dataset1);
  
  dataset2.SetTitle(priority + " PriorityQueueThreshold");
  dataset2.SetStyle(Gnuplot2dDataset::LINES);
  // load a dat file as data set for plotting
  std::ifstream file2(location + "TrafficControl" + priority + "PriorityQueueThreshold_" + portInd + ".dat");
  x = 0;
  y = 0;
  while (file2 >> x >> y) 
  {
    dataset2.Add(x, y);
  }
  // Add the dataset to the plot.
  plot.AddDataset(dataset2);

  // Open the plot file.
  std::ofstream plotFile(plotFileName + ".plt");
  // Write the plot file.
  plot.GenerateOutput(plotFile);
  // Close the plot file.
  plotFile.close();
  // command line needs to be in ./ns-3-dev-git$ inorder for the script to produce gnuplot correctly///
  system (("gnuplot " + plotFileName + ".plt").c_str ());
}

void
SojournTimeTrace (Time sojournTime)
{
  std::cout << "Sojourn time " << sojournTime.ToDouble (Time::MS) << "ms" << std::endl;
}


int main (int argc, char *argv[])
{
  LogComponentEnable ("1In1Out", LOG_LEVEL_INFO);

  double_t miceElephantProb = 0.2;
  // predictive model should behaive just like the regular model would 
  double_t alpha_high = 15;
  double_t alpha_low = 5;
  
  double_t future_possition = 0.5; // the possition of the estimation window in regards of past time samples/future samples.
  double_t win_length = 0.4; // estimation window length in time [sec]
  std::string applicationType = "prioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"prioOnOff"
  std::string transportProt = "TCP"; // "UDP"/"TCP"
  std::string socketType;
  std::string queue_capacity;
  
  // this file is for debug, to trace the sequence of all estimated d values during the run
  // it needs to be erased before the simulation starts
  if (std::filesystem::exists("Estimated_D_Values.dat"))
  {
    std::remove("Estimated_D_Values.dat");
  }

  // Create a new directory to store the output of the program
  // datDir = "./Trace_Plots/1In1Out/Predictive/";
  std::string  dir = datDir + "Position_" + DoubleToString(future_possition) + "/Length_" + DoubleToString(win_length) + "/";
  std::string dirToSave = dir + traffic_control_type + "/" + implementation + "/" + applicationType + "/" + onOffTrafficMode;
  if (!((std::filesystem::exists(dirToSave)) && (std::filesystem::is_directory(dirToSave))))
  {
    system (("mkdir -p " + dirToSave).c_str ());
  }

  // // set global runDir so trace callbacks write into this per-run directory
  // runDir = dirToSave;

  uint32_t flowletTimeout = 50;
  bool eraseOldData = true; // true/false

  if (eraseOldData == true)
  {
    system (("rm " + dirToSave + "/*.dat").c_str ()); // to erase the old .dat files and collect new data
    system (("rm " + dirToSave + "/*.txt").c_str ()); // to erase the previous test run summary, and collect new data
    system (("rm " + dirToSave + "/*.plt").c_str ()); // to erase the previous test plot related files
    system (("rm " + dirToSave + "/*.png").c_str ()); // to erase the previous test plot related files
    std::cout << std::endl << "***Erased Previous Data***\n" << std::endl;
  }

  NS_LOG_INFO ("Config parameters");

  CommandLine cmd;
  cmd.AddValue ("transportProt", "Transport protocol to use: Udp, Tcp, DcTcp", transportProt);
  cmd.AddValue ("flowletTimeout", "flowlet timeout", flowletTimeout);
  cmd.Parse (argc, argv);

  if (traffic_control_type.compare("SharedBuffer_DT") == 0)
  {
    usedAlgorythm = "PredictiveDT";
  }
  else if (traffic_control_type.compare("SharedBuffer_FB") == 0)
  {
    usedAlgorythm = "PredictiveFB";
  }

  // Application type dependent parameters
  if (applicationType.compare("standardClient") == 0 || applicationType.compare("prioClient") == 0)
  {
    queue_capacity = "20p"; // B, the total space on the buffer.
  }
  else
  {
    queue_capacity = ToString(RECIEVER_COUNT * BUFFER_SIZE) + "p"; // B, the total space on the buffer [packets]
  }

  // client type dependant parameters:
  if (transportProt.compare ("TCP") == 0)
  {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno")); // "ns3::TcpNewReno"/"ns3::TcpNoCongestion"
    Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue("Off"));
    Config::SetDefault("ns3::TcpL4Protocol::RecoveryType",
                       TypeIdValue(TypeId::LookupByName("ns3::TcpClassicRecovery")));
    
    // Enlarge TCP send/receive buffers to avoid rwnd (flow-control) limiting BytesInFlight
    // Default RcvBufSize in ns-3 is 131072 bytes, which capped BytesInFlight at 128 KiB in our runs.
    // Use large buffers so advertised window (and rwnd) can grow well beyond the shared-buffer capacity.
    // Note: Window scaling is enabled by default (TcpSocketBase::m_winScalingEnabled = true);
    // these sizes ensure the scaled window is sufficiently large.
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 26)); // 64 MiB
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 26)); // 64 MiB
    
    // === Prevent spurious retransmits with user-specified settings ===
  
    // 1. Set very conservative RTO bounds to avoid premature timeouts in high-delay environment
    Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(Seconds(5.0)));     // Min RTO = 5s (extra conservative for wireless)
    Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue(MilliSeconds(10))); // 10ms granularity (coarser for stability)
    
    // 2. Configure RTT estimation to be more conservative in wireless environment
    Config::SetDefault("ns3::RttMeanDeviation::Alpha", DoubleValue(0.0625));  // Slower SRTT adaptation (default 0.125)
    Config::SetDefault("ns3::RttMeanDeviation::Beta", DoubleValue(0.125));    // Slower RTTVAR adaptation (default 0.25)
    
    // === User-specified TCP settings ===
  
    // 3. User-requested segment size and initial congestion window
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(PACKET_SIZE + 60)); // add 60 bytes for TCP/IP headers
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1000));             // User-requested large initial window
    
    // 4. Enable TCP timestamps as requested (helps with RTT measurement and PAWS)
    Config::SetDefault("ns3::TcpSocketBase::Timestamp", BooleanValue(true));        // Enable timestamps for better RTT estimation
    Config::SetDefault("ns3::TcpSocketBase::WindowScaling", BooleanValue(true));    // Keep window scaling (needed for large buffers)
    
    // === Anti-spurious retransmission optimizations ===
  
    // 6. Conservative delayed ACK to reduce reverse traffic and potential ACK compression
    Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(500))); // Longer delay to avoid ACK storms
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(4));               // Wait for more segments before ACKing

    // 7. Enable advanced recovery mechanisms
    Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(true));             // Enable SACK for better recovery
    Config::SetDefault("ns3::TcpSocketBase::LimitedTransmit", BooleanValue(false)); // Disable limited transmit to reduce spurious sends

    // 8. Conservative data retry settings
    Config::SetDefault("ns3::TcpSocket::DataRetries", UintegerValue(12));           // More retries before giving up

    // 9. Additional anti-spurious optimizations
    Config::SetDefault("ns3::TcpSocket::TcpNoDelay", BooleanValue(false));          // Enable Nagle's algorithm to reduce small packets
    Config::SetDefault("ns3::TcpSocket::PersistTimeout", TimeValue(Seconds(10))); // Longer persist timeout for window probes

    // 10. Connection establishment timeout (reduce SYN retransmissions)
    Config::SetDefault("ns3::TcpSocket::ConnTimeout", TimeValue(Seconds(5)));      // Longer connection timeout
    Config::SetDefault("ns3::TcpSocket::ConnCount", UintegerValue(3));             // Fewer SYN retransmission attempts
    
    socketType = "ns3::TcpSocketFactory";
  }
  else
  {
    socketType = "ns3::UdpSocketFactory";
  }

  // Application and Client type dependent parameters
  // select the desired components to output data
  if (applicationType.compare("standardClient") == 0 && transportProt.compare ("TCP") == 0)
  {
      LogComponentEnable ("TcpClient", LOG_LEVEL_INFO);
  }
  else if (applicationType.compare("standardClient") == 0 && transportProt.compare ("UDP") == 0)
  {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  }
  else if ((applicationType.compare("OnOff") == 0 || applicationType.compare("priorityOnOff") == 0 || applicationType.compare("priorityApplication") == 0)&& transportProt.compare ("Tcp") == 0)
  {
      LogComponentEnable("TcpSocketImpl", LOG_LEVEL_INFO);
  }
  else if ((applicationType.compare("OnOff") == 0 || applicationType.compare("priorityOnOff") == 0 || applicationType.compare("priorityApplication") == 0) && transportProt.compare ("Udp") == 0)
  {
      LogComponentEnable("UdpSocketImpl", LOG_LEVEL_INFO);
  }

  LogComponentEnable("PacketSink", LOG_LEVEL_INFO); 
  
  NS_LOG_INFO ("Create nodes");
  NodeContainer recievers;
  recievers.Create (RECIEVER_COUNT);
  NodeContainer router;
  router.Create (SWITCH_COUNT);
  // for loop use. make sure name "Router" is not stored in Names map///
  Names::Clear();
  Names::Add("Router", router.Get(0));  // Add a Name to the router node
  NodeContainer servers;
  servers.Create (SERVER_COUNT);

  // for predictive model
  NodeContainer recieversPredict;
  recieversPredict.Create (RECIEVER_COUNT);
  NodeContainer routerPredict;
  routerPredict.Create (SWITCH_COUNT);
  Names::Add("RouterPredict", routerPredict.Get(0));  // Add a Name to the router node
  NodeContainer serversPredict;
  serversPredict.Create (SERVER_COUNT);


  PointToPointHelper n2s, s2r, n2sPredict, s2rPredict;
  NS_LOG_INFO ("Configuring channels for all the Nodes");
  
  // Setting servers
  uint64_t serverSwitchCapacity = SERVER_SWITCH_CAPACITY;
  n2s.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (serverSwitchCapacity)));
  n2s.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
  n2s.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));  // set basic queues to 1 packet
  // setting routers
  uint64_t switchRecieverCapacity = SWITCH_RECIEVER_CAPACITY;
  s2r.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (switchRecieverCapacity)));
  s2r.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
  s2r.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));  // set basic queues to 1 packet

  //create additional system identical to the original Buffer to simulate traffic for predictive analitics
  NS_LOG_INFO ("Configuring Predictive Nodes");
  // Setting servers
  n2sPredict.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (serverSwitchCapacity)));
  n2sPredict.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
  n2sPredict.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));  // set basic queues to 1 packet
  // setting routers
  s2rPredict.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (switchRecieverCapacity)));
  s2rPredict.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
  s2rPredict.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));  // set basic queues to 1 packet

  NS_LOG_INFO ("Create NetDevices");
  NetDeviceContainer serverDevices;
  NetDeviceContainer switchDevicesIn;
  NetDeviceContainer switchDevicesOut;
  NetDeviceContainer recieverDevices;
  // for predictive model:
  NetDeviceContainer serverDevicesPredict;
  NetDeviceContainer switchDevicesInPredict;
  NetDeviceContainer switchDevicesOutPredict;
  NetDeviceContainer recieverDevicesPredict;

  NS_LOG_INFO ("Install NetDevices on all Nodes");
  NS_LOG_INFO ("Configuring Servers");
  for (size_t i = 0; i < SERVER_COUNT; i++)
  {
    NS_LOG_INFO ("Server " << i << " is connected to switch");

    NetDeviceContainer tempNetDevice = n2s.Install (servers.Get (i), router.Get(0));
    serverDevices.Add(tempNetDevice.Get(0));
    switchDevicesIn.Add(tempNetDevice.Get(1));

    // for predictive model:
    NetDeviceContainer tempNetDevicePredict = n2sPredict.Install (serversPredict.Get (i), routerPredict.Get(0));
    serverDevicesPredict.Add(tempNetDevicePredict.Get(0));
    switchDevicesInPredict.Add(tempNetDevicePredict.Get(1));
  }
  
  NS_LOG_INFO ("Configuring switches");

  for (size_t i = 0; i < RECIEVER_COUNT; i++)
  {
    NetDeviceContainer tempNetDevice = s2r.Install (router.Get(0), recievers.Get (i));
    switchDevicesOut.Add(tempNetDevice.Get(0));
    recieverDevices.Add(tempNetDevice.Get(1));

    // for predictive model:
    NetDeviceContainer tempNetDevicePredict = s2rPredict.Install (routerPredict.Get(0), recieversPredict.Get (i));
    switchDevicesOutPredict.Add(tempNetDevicePredict.Get(0));
    recieverDevicesPredict.Add(tempNetDevicePredict.Get(1));

    NS_LOG_INFO ("Switch is connected to Reciever " << i << "at capacity: " << switchRecieverCapacity);     
  }

  // add a "name" to the "switchDeviceOut" and "switchDeviceOutPredict" NetDevices
  for (size_t i = 0; i < switchDevicesOut.GetN(); i++) 
  {     
    // Add a Name to the switch net-devices
    Names::Add("switchDeviceOut" + IntToString(i), switchDevicesOut.Get(i));  
    Names::Add("switchDeviceOutPredict" + IntToString(i), switchDevicesOutPredict.Get(i));
  }

  // Now add ip/tcp stack to all nodes. this is a VERY IMPORTANT COMPONENT!!!!
  NS_LOG_INFO ("Install Internet stacks");
  InternetStackHelper internet;
  internet.InstallAll ();

  NS_LOG_INFO ("Install QueueDisc");

  TrafficControlHelper tch;
  // priomap with low priority for class "0" and high priority for rest of the 15 classes (1-15). Technically not nesessary for RoundRobinPrioQueue
  std::array<uint16_t, 16> prioArray = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  
  // Priomap prioMap = Priomap{prioArray};
  TosMap tosMap = TosMap{prioArray};
  uint16_t handle = tch.SetRootQueueDisc("ns3::RoundRobinTosQueueDisc", "TosMap", TosMapValue(tosMap));

  TrafficControlHelper::ClassIdList cid = tch.AddQueueDiscClasses(handle, 2, "ns3::QueueDiscClass");
  tch.AddChildQueueDisc(handle, cid[0], "ns3::FifoQueueDisc" , "MaxSize", StringValue (queue_capacity)); // cid[0] is band "0" - the Highest Priority band
  tch.AddChildQueueDisc(handle, cid[1], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[1] is Low Priority

  // in this option we installed TCH on switchDevicesOut. to send data from switch to reciever
  QueueDiscContainer qdiscs = tch.Install (switchDevicesOut);  
  // for predictive model Install Traffic Control Helper on the Predictive model
  QueueDiscContainer qdiscsPredict = tch.Install(switchDevicesOutPredict);

  ///////// set the Traffic Controll layer to be a shared buffer////////////////////////
  TcPriomap tcPrioMap = TcPriomap{prioArray};
  Ptr<TrafficControlLayer> tc;
  tc = router.Get(0)->GetObject<TrafficControlLayer>();
  tc->SetAttribute("SharedBuffer", BooleanValue(true));
  tc->SetAttribute("MaxSharedBufferSize", StringValue (queue_capacity));
  tc->SetAttribute("Alpha_High", DoubleValue (alpha_high));
  tc->SetAttribute("Alpha_Low", DoubleValue (alpha_low));
  tc->SetAttribute("TrafficControlAlgorythm", StringValue (usedAlgorythm));
  tc->SetAttribute("PriorityMapforMultiQueue", TcPriomapValue(tcPrioMap));
  tc->SetAttribute("TrafficEstimationWindowLength", DoubleValue(win_length));

  // monitor the packets in the Shared Buffer in Traffic Control Layer:
  tc->TraceConnectWithoutContext("PacketsInQueue", MakeCallback (&TrafficControlPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("HighPriorityPacketsInQueue", MakeCallback (&TrafficControlHighPriorityPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("LowPriorityPacketsInQueue", MakeCallback (&TrafficControlLowPriorityPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_High_0", MakeBoundCallback (&TrafficControlThresholdHighTrace, 0));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_Low_0", MakeBoundCallback (&TrafficControlThresholdLowTrace, 0));  
  tc->TraceConnectWithoutContext("EnqueueingThreshold_High_1", MakeBoundCallback (&TrafficControlThresholdHighTrace, 1));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_Low_1", MakeBoundCallback (&TrafficControlThresholdLowTrace, 1));
  
  // for predictive model
  Ptr<TrafficControlLayer> tcPredict;
  tcPredict = routerPredict.Get(0)->GetObject<TrafficControlLayer>();
  tcPredict->SetAttribute("SharedBuffer", BooleanValue(true));
  tcPredict->SetAttribute("PriorityMapforMultiQueue", TcPriomapValue(tcPrioMap));
  if (transportProt.compare ("UDP") == 0)
  {
    tcPredict->SetAttribute("MaxSharedBufferSize", StringValue ("1p")); // no packets are actualy being stored in tcPredict
  }
  else
  {
    // the Predictive model will function just like a real non-predictive Shared Buffer would
    tcPredict->SetAttribute("MaxSharedBufferSize", StringValue (queue_capacity));
    tcPredict->SetAttribute("Alpha_High", DoubleValue (alpha_high));
    tcPredict->SetAttribute("Alpha_Low", DoubleValue (alpha_low)); 
    // TrafficControlAlgorythm is the non-predictive version of the TrafficControlAlgorythm
    tcPredict->SetAttribute("TrafficControlAlgorythm", StringValue (usedAlgorythm.substr(usedAlgorythm.length() - 2)));
  }

  ///////////Monitor data from q-disc//////////////////////////////////////////
  for (size_t i = 0; i < RECIEVER_COUNT; i++)  // over all ports
  {
    for (size_t j = 0; j < qdiscs.Get (i)->GetNQueueDiscClasses(); j++)  // over all the queues per port
    {
      Ptr<QueueDisc> queue = qdiscs.Get (i)->GetQueueDiscClass(j)->GetQueueDisc();
      queue->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&MultiQueueDiscPacketsInQueueTrace, i, j));
      // queue->TraceConnectWithoutContext ("HighPriorityPacketsInQueue", MakeBoundCallback (&HighPriorityMultiQueueDiscPacketsInQueueTrace, i, j)); 
      // queue->TraceConnectWithoutContext ("LowPriorityPacketsInQueue", MakeBoundCallback (&LowPriorityMultiQueueDiscPacketsInQueueTrace, i, j)); 
    }
  }

  NS_LOG_INFO ("Setup IPv4 Addresses");

  ns3::Ipv4AddressHelper server2switchIPs =
      ns3::Ipv4AddressHelper("10.1.0.0", "255.255.255.0");
  ns3::Ipv4AddressHelper switch2recieverIPs =
      ns3::Ipv4AddressHelper("10.2.0.0", "255.255.255.0");


  Ipv4InterfaceContainer serverIFs;
  Ipv4InterfaceContainer switchInIFs;
  Ipv4InterfaceContainer switchOutIFs;
  Ipv4InterfaceContainer recieverIFs;
  // for predictive model
  Ipv4InterfaceContainer serverIFsPredict;
  Ipv4InterfaceContainer switchInIFsPredict;
  Ipv4InterfaceContainer switchOutIFsPredict;
  Ipv4InterfaceContainer recieverIFsPredict;



  NS_LOG_INFO ("Install IPv4 addresses on all NetDevices");
  
  NS_LOG_INFO ("Configuring servers");

  for (size_t i = 0; i < SERVER_COUNT; i++)
  {
    NetDeviceContainer tempNetDevice;
    tempNetDevice.Add(serverDevices.Get(i));
    tempNetDevice.Add(switchDevicesIn.Get(i));
    Ipv4InterfaceContainer ifcServers = server2switchIPs.Assign(tempNetDevice);
    serverIFs.Add(ifcServers.Get(0));
    switchInIFs.Add(ifcServers.Get(1));
    server2switchIPs.NewNetwork ();

    //for prdictive model
    NetDeviceContainer tempNetDevicePredict;
    tempNetDevicePredict.Add(serverDevicesPredict.Get(i));
    tempNetDevicePredict.Add(switchDevicesInPredict.Get(i));
    Ipv4InterfaceContainer ifcServersPredict = server2switchIPs.Assign(tempNetDevicePredict);
    serverIFsPredict.Add(ifcServersPredict.Get(0));
    switchInIFsPredict.Add(ifcServersPredict.Get(1));
    server2switchIPs.NewNetwork ();
  }

  NS_LOG_INFO ("Configuring switch");

  for (size_t i = 0; i < RECIEVER_COUNT; i++)
  {
    NetDeviceContainer tempNetDevice;
    tempNetDevice.Add(switchDevicesOut.Get(i));
    tempNetDevice.Add(recieverDevices.Get(i));
    Ipv4InterfaceContainer ifcSwitch = switch2recieverIPs.Assign(tempNetDevice);
    switchOutIFs.Add(ifcSwitch.Get(0));
    recieverIFs.Add(ifcSwitch.Get(1));
    switch2recieverIPs.NewNetwork ();

    //for prdictive model        
    NetDeviceContainer tempNetDevicePredict;
    tempNetDevicePredict.Add(switchDevicesOutPredict.Get(i));
    tempNetDevicePredict.Add(recieverDevicesPredict.Get(i));
    Ipv4InterfaceContainer ifcSwitchPredict = switch2recieverIPs.Assign(tempNetDevicePredict);
    switchOutIFsPredict.Add(ifcSwitchPredict.Get(0));
    recieverIFsPredict.Add(ifcSwitchPredict.Get(1));
    switch2recieverIPs.NewNetwork ();    
  }


  // and setup ip routing tables to get total ip-level connectivity.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Sockets, Applications and Sinks");

  // uint32_t ipTos_HP = 0x10;  //High priority: Maximize Throughput
  uint32_t ipTos_LP = 0x00; //Low priority: Best Effort
  // uint32_t ipTos_LP = 0x06; //Low priority: Best Effort
  
  ApplicationContainer sinkApps, sourceApps, sourceAppsPredict, sinkAppsPredict;
  // time interval values for OnOff Aplications
  double_t elephantOnTime = trafficGenDuration; // [sec]
  double_t elephantOffTime = 0.0; // [sec]
  // for RNG:
  double_t elephantOnTimeMax = 2 * elephantOnTime; // [sec]
  double_t elephantOffTimeMax = 2 * elephantOffTime; // [sec]
  
  std::remove("TCP_Socket_Priority_per_Port.dat"); // remove it here, to create a new one with every run

  for (size_t i = 0; i < SERVER_COUNT; i++)
  {
    size_t serverIndex = i;
    size_t recieverIndex = i;
    // create sockets
    ns3::Ptr<ns3::Socket> sockptr, sockptrPredict;

    if (transportProt.compare("TCP") == 0) 
    {
      // setup source socket
      sockptr =
          ns3::Socket::CreateSocket(servers.Get(serverIndex),
                  ns3::TcpSocketFactory::GetTypeId());
      ns3::Ptr<ns3::TcpSocket> tcpsockptr =
          ns3::DynamicCast<ns3::TcpSocket> (sockptr);

      // setup source socket for predictive model
      sockptrPredict =
          ns3::Socket::CreateSocket(serversPredict.Get(serverIndex),
                  ns3::TcpSocketFactory::GetTypeId());
      ns3::Ptr<ns3::TcpSocket> tcpsockptrPredict =
          ns3::DynamicCast<ns3::TcpSocket> (sockptrPredict);
    } 
    else if (transportProt.compare("UDP") == 0) 
    {
      // setup source socket
      sockptr =
          ns3::Socket::CreateSocket(servers.Get(serverIndex),
                  ns3::UdpSocketFactory::GetTypeId());
      ////////Added by me///////////////        
      ns3::Ptr<ns3::UdpSocket> udpsockptr =
          ns3::DynamicCast<ns3::UdpSocket> (sockptr);
          //////////////////////////////////
      
      // setup source socket for predictive model
      sockptrPredict =
          ns3::Socket::CreateSocket(serversPredict.Get(serverIndex),
                  ns3::UdpSocketFactory::GetTypeId());
      ////////Added by me///////////////        
      ns3::Ptr<ns3::UdpSocket> udpsockptrPredict =
          ns3::DynamicCast<ns3::UdpSocket> (sockptrPredict);
      //////////////////////////////////

    } 
    else 
    {
      std::cerr << "unknown transport type: " <<
          transportProt << std::endl;
      exit(1);
    }
    
    InetSocketAddress socketAddressP1 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P1);
    socketAddressP1.SetTos(ipTos_LP);  // ToS 0x0 -> Low priority

    // for preddictive model
    InetSocketAddress socketAddressP1Predict = InetSocketAddress (recieverIFsPredict.GetAddress(recieverIndex), SERV_PORT_P1);
    socketAddressP1Predict.SetTos(ipTos_LP);  // ToS 0x0 -> Low priority
    
    
    
    // create and install Client apps:    
    
    if (applicationType.compare("prioOnOff") == 0) 
    {
      // Create the OnOff applications to send TCP/UDP to the server

      PrioOnOffHelper clientHelperP1 (socketType, socketAddressP1);
      clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
      if (onOffTrafficMode.compare("Constant") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
      }
      else if (onOffTrafficMode.compare("Uniform") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
      }
      else if (onOffTrafficMode.compare("Normal") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
      }
      else 
      {
          std::cerr << "unknown OnOffMode type: " << onOffTrafficMode << std::endl;
          exit(1);
      }
      clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP1.SetAttribute ("DataRate", StringValue ("2Mb/s"));
      // clientHelperP1.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP1.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
      
      clientHelperP1.SetAttribute ("ApplicationToS", UintegerValue (ipTos_LP)); // set the IP ToS value for the application
      // clientHelpers_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP1.SetAttribute("MiceElephantProbability", StringValue (DoubleToString(miceElephantProb)));
      clientHelperP1.SetAttribute("StreamIndex", UintegerValue (1 + 2*(i + 1))); // assign a stream for RNG for each OnOff application instance
      
      sourceApps.Add(clientHelperP1.Install (servers.Get(serverIndex)));
      // clientHelperP1.AssignStreams(servers, 1);

      // for predicting traffic in queue

      PrioOnOffHelper clientHelperP1Predict (socketType, socketAddressP1Predict);
      clientHelperP1Predict.SetAttribute ("Remote", AddressValue (socketAddressP1Predict));
      if (onOffTrafficMode.compare("Constant") == 0)
      {
        clientHelperP1Predict.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP1Predict.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
      }
      else if (onOffTrafficMode.compare("Uniform") == 0)
      {
        clientHelperP1Predict.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
        clientHelperP1Predict.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
      }
      else if (onOffTrafficMode.compare("Normal") == 0)
      {
        clientHelperP1Predict.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP1Predict.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
      }
      else 
      {
          std::cerr << "unknown OnOffMode type: " << onOffTrafficMode << std::endl;
          exit(1);
      }
      clientHelperP1Predict.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP1Predict.SetAttribute ("DataRate", StringValue ("2Mb/s"));
      // clientHelperP1Predict.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP1Predict.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
      
      clientHelperP1Predict.SetAttribute ("ApplicationToS", UintegerValue (ipTos_LP)); // set the IP ToS value for the application
      // clientHelpers_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP1Predict.SetAttribute("MiceElephantProbability", StringValue (DoubleToString(miceElephantProb)));
      clientHelperP1Predict.SetAttribute("StreamIndex", UintegerValue (1 + 2*(i + 1))); // assign a stream for RNG for each OnOff application instance

      sourceAppsPredict.Add(clientHelperP1Predict.Install (serversPredict.Get(serverIndex)));
      // clientHelperP1Predict.AssignStreams(serversPredict, 1);

      // for TCP control packets. need to map the Rx Port to the priority of the OnOff Application
      // {[nodeID] [port] [priority]}
      std::ofstream tcpPriorityPerPortOutputFile("TCP_Socket_Priority_per_Port.dat", std::ios::app);
      tcpPriorityPerPortOutputFile << servers.Get(i)->GetId() << " " << SERV_PORT_P1 << " " << 2 << std::endl; 
      tcpPriorityPerPortOutputFile << serversPredict.Get(i)->GetId() << " " << SERV_PORT_P1 << " " << 2 << std::endl;
      tcpPriorityPerPortOutputFile.close();
    }
    else 
    {
      std::cerr << "unknown app type: " << applicationType << std::endl;
      exit(1);
    }
    // setup sinks
    Address sinkLocalAddressP1 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P1));
    PacketSinkHelper sinkP1 (socketType, sinkLocalAddressP1); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    sinkApps.Add(sinkP1.Install (recievers.Get(recieverIndex)));
    // for Predictive model - needed for TCP
    Address sinkLocalAddressPredictP1 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P1));
    PacketSinkHelper sinkPredictP1 (socketType, sinkLocalAddressPredictP1); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    sinkAppsPredict.Add(sinkPredictP1.Install (recieversPredict.Get(recieverIndex)));
  }

  // double_t trafficGenDuration = 2; // for a single OnOff segment
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds(1.0 + trafficGenDuration));

  // start predictive model at t0 - Tau
  sourceAppsPredict.Start (Seconds (1.0 - win_length*future_possition));
  sourceAppsPredict.Stop (Seconds(1.0 + trafficGenDuration - win_length*(1 - future_possition)));

  sinkApps.Start (Seconds (START_TIME));
  sinkApps.Stop (Seconds (END_TIME + 0.1));

  sinkAppsPredict.Start (Seconds (START_TIME));
  sinkAppsPredict.Stop (Seconds (END_TIME + 0.1));

  //monitor OnOff Traffic generated from the OnOff Applications:
  Simulator::Schedule (Seconds (1.0), &returnOnOffTraffic, sourceApps, 3.1, Simulator::Now().GetSeconds());
  
  NS_LOG_INFO ("Enabling flow monitor");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  // ns3::NodeContainer activeTrafficNodes;
  // activeTrafficNodes.Add(servers, router, recievers);
  // Ptr<FlowMonitor> monitor = flowmon.Install(activeTrafficNodes);

  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (END_TIME + 10));
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl << "Topology: 1In1Out" << std::endl;
  std::cout << std::endl << "Queueing Algorithm: " + traffic_control_type + "Predictive" << std::endl;
  std::cout << std::endl << "Implementation Method: " + implementation << std::endl;
  std::cout << std::endl << "Used D value: " + DoubleToString(miceElephantProb) << std::endl;
  std::cout << std::endl << "Traffic Duration: " + DoubleToString(trafficGenDuration) + " [Sec]" << std::endl;
  std::cout << std::endl << "Application: " + applicationType << std::endl;
  std::cout << std::endl << "traffic Mode " + onOffTrafficMode + "Random" << std::endl;

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
  // a loop to sum the Tx/Rx Packets/Bytes from all nodes
  uint32_t statTxPackets = 0; 
  uint64_t statTxBytes = 0;
  uint32_t statRxPackets = 0; 
  uint64_t statRxBytes = 0;

  uint32_t packetsDroppedByQueueDisc = 0;
  uint64_t bytesDroppedByQueueDisc = 0;

  uint32_t packetsDroppedByNetDevice = 0;
  uint64_t bytesDroppedByNetDevice = 0;

  double TpT = 0;

  double AVGDelaySum = 0;
  double AVGDelay = 0;

  double AVGJitterSum = 0;
  double AVGJitter = 0;

  uint64_t totalBytesRx = 0;
  double goodTpT = 0;
  // NEED TO ADJUST STATISTICS SUCH THAT IT COUNTS ONLY REAL DATA SENT, AND NOT PREDICTIVE DATA.
  // (i.e. packets sent by the actual model and not the predictive packets)
  // stats indexing needs to start from 1. 
  // first half of the flows are the predictive flows
  // if TCP: flowStats[i].protocol == "TCP"
  // first half of each group (predictive/real) is the actual DATA traffic. the second half is the back traffic of ACKs
  if (transportProt.compare("TCP") == 0)
  {
    for (size_t i = flowStats.size()/2 + 1; i <= flowStats.size() - flowStats.size()/4; i++) 
    {
      if (classifier->FindFlow(i).protocol == 6) // TCP protocol number is 6
      {
        statTxPackets = statTxPackets + flowStats[i].txPackets;
        statTxBytes = statTxBytes + flowStats[i].txBytes;
        statRxPackets = statRxPackets + flowStats[i].rxPackets;
        statRxBytes = statRxBytes + flowStats[i].rxBytes;
      }
      if (flowStats[i].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE_DISC)
      {
        packetsDroppedByQueueDisc = packetsDroppedByQueueDisc + flowStats[i].packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
        bytesDroppedByQueueDisc = bytesDroppedByQueueDisc + flowStats[i].bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
      }
      if (flowStats[i].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE)
      {
        packetsDroppedByNetDevice = packetsDroppedByNetDevice + flowStats[i].packetsDropped[Ipv4FlowProbe::DROP_QUEUE];
        bytesDroppedByNetDevice = bytesDroppedByNetDevice + flowStats[i].bytesDropped[Ipv4FlowProbe::DROP_QUEUE];
      }
      TpT = TpT + (flowStats[i].rxBytes * 8.0 / (flowStats[i].timeLastRxPacket.GetSeconds () - flowStats[i].timeFirstRxPacket.GetSeconds ())) / 1000000;
      AVGDelaySum = AVGDelaySum + flowStats[i].delaySum.GetSeconds () / flowStats[i].rxPackets;
      AVGJitterSum = AVGJitterSum + flowStats[i].jitterSum.GetSeconds () / (flowStats[i].rxPackets - 1);
    }
  }
  else // UDP
  {
    for (size_t i = flowStats.size()/2 + 1; i <= flowStats.size(); i++) 
    {
      if (classifier->FindFlow(i).protocol == 17) // UDP protocol number is 17
      {
        statTxPackets = statTxPackets + flowStats[i].txPackets;
        statTxBytes = statTxBytes + flowStats[i].txBytes;
        statRxPackets = statRxPackets + flowStats[i].rxPackets;
        statRxBytes = statRxBytes + flowStats[i].rxBytes;
      }
      if (flowStats[i].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE_DISC)
      {
        packetsDroppedByQueueDisc = packetsDroppedByQueueDisc + flowStats[i].packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
        bytesDroppedByQueueDisc = bytesDroppedByQueueDisc + flowStats[i].bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
      }
      if (flowStats[i].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE)
      {
        packetsDroppedByNetDevice = packetsDroppedByNetDevice + flowStats[i].packetsDropped[Ipv4FlowProbe::DROP_QUEUE];
        bytesDroppedByNetDevice = bytesDroppedByNetDevice + flowStats[i].bytesDropped[Ipv4FlowProbe::DROP_QUEUE];
      }
      TpT = TpT + (flowStats[i].rxBytes * 8.0 / (flowStats[i].timeLastRxPacket.GetSeconds () - flowStats[i].timeFirstRxPacket.GetSeconds ())) / 1000000;
      AVGDelaySum = AVGDelaySum + flowStats[i].delaySum.GetSeconds () / flowStats[i].rxPackets;
      AVGJitterSum = AVGJitterSum + flowStats[i].jitterSum.GetSeconds () / (flowStats[i].rxPackets - 1);
    }
  }

  std::cout << "  Tx Packets/Bytes:   " << statTxPackets
              << " / " << statTxBytes << std::endl;
  std::cout << "  Rx Packets/Bytes:   " << statRxPackets
              << " / " << statRxBytes << std::endl;

  std::cout << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc
              << " / " << bytesDroppedByQueueDisc << std::endl;
  
  std::cout << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice
              << " / " << bytesDroppedByNetDevice << std::endl;
  
  std::cout << "  Throughput: " << TpT << " Mbps" << std::endl;
                                  
  AVGDelay = AVGDelaySum / (flowStats.size()/2);
  std::cout << "  Mean delay:   " << AVGDelay << std::endl;
  
  AVGJitter = AVGJitterSum / (flowStats.size()/2);
  std::cout << "  Mean jitter:   " << AVGJitter << std::endl;
  
  std::cout << std::endl << "*** Application statistics ***" << std::endl;

  // sinkApps refer to traffic sinks for real data only (not predictive data)
  for (size_t i = 0; i < sinkApps.GetN(); i++)
  {
    totalBytesRx = totalBytesRx + DynamicCast<PacketSink> (sinkApps.Get (i))->GetTotalRx ();
  }

  goodTpT = totalBytesRx * 8 / (END_TIME * 1000000.0); //Mbit/s
  std::cout << "  Rx Bytes: " << totalBytesRx << std::endl;
  std::cout << "  Average Goodput: " << goodTpT << " Mbit/s" << std::endl;

  TrafficControlLayer::TCStats tcStats = tc->GetStats();
  std::cout << std::endl << "*** TC Layer statistics ***" << std::endl;
  std::cout << tcStats << std::endl;
  
  std::cout << std::endl << "*** QueueDisc statistics ***" << std::endl;
  for (size_t i = 0; i < qdiscs.GetN(); i++)
  {
    std::cout << "Queue Disceplene " << i << ":" << std::endl;
    std::cout << qdiscs.Get(i)->GetStats () << std::endl;
  }

  
  // Added to create a .txt file with the summary of the tested scenario statistics
  std::ofstream testFlowStatistics (dirToSave + "/Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: 1In1Out" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + traffic_control_type + "Predictive" << std::endl;
  testFlowStatistics << "Implementation Method: " + implementation << std::endl;
  testFlowStatistics << "Used D value: " + DoubleToString(miceElephantProb) << std::endl;
  testFlowStatistics << "Traffic Duration: " + DoubleToString(trafficGenDuration) + " [Sec]" << std::endl;
  testFlowStatistics << "Application: " + applicationType << std::endl;
  testFlowStatistics << "traffic Mode " + onOffTrafficMode + "Random" << std::endl; 
  testFlowStatistics << std::endl << "*** Flow monitor statistics ***" << std::endl;
  testFlowStatistics << "  Tx Packets/Bytes:   " << statTxPackets << " / " << statTxBytes << std::endl;
  testFlowStatistics << "  Rx Packets/Bytes:   " << statRxPackets << " / " << statRxBytes << std::endl;
  testFlowStatistics << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc
                      << " / " << bytesDroppedByQueueDisc << std::endl;
  testFlowStatistics << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice
                      << " / " << bytesDroppedByNetDevice << std::endl;
  testFlowStatistics << "  Throughput: " << TpT << " Mbps" << std::endl; 
  testFlowStatistics << std::endl << "*** Application statistics ***" << std::endl;
  testFlowStatistics << "  Rx Bytes: " << totalBytesRx << std::endl;
  testFlowStatistics << "  Average Goodput: " << goodTpT << " Mbit/s" << std::endl;                  
  testFlowStatistics << std::endl << "*** TC Layer statistics ***" << std::endl;
  testFlowStatistics << tcStats << std::endl;
  testFlowStatistics << std::endl << "*** QueueDisc Layer statistics ***" << std::endl;
  for (size_t i = 0; i < qdiscs.GetN(); i++)
  {
    testFlowStatistics << "Queue Disceplene " << i << ":" << std::endl;
    testFlowStatistics << qdiscs.Get(i)->GetStats () << std::endl;
  }
  
  testFlowStatistics.close ();

  // move all the produced files to dirToSave
  system (("mv -f " + datDir + "/*.dat " + dirToSave).c_str ());
  // system (("mv -f " + dir + traffic_control_type + "/" + implementation + + "/*.txt " + dirToSave).c_str ());

  // Create the gnuplot.//////////////////////////////
  generate1DGnuPlotFromDatFile(dirToSave + "/generatedOnOffTrafficTrace.dat");
  // generate1DGnuPlotFromDatFile(dirToSave + "/non-predictive_port_0_app_1_OnOffStateTrace.dat");
  // generate1DGnuPlotFromDatFile(dirToSave + "/non-predictive_port_1_app_1_OnOffStateTrace.dat");
  // generate1DGnuPlotFromDatFile(dirToSave + "/predictive_port_0_app_1_OnOffStateTrace.dat");
  // generate1DGnuPlotFromDatFile(dirToSave + "/predictive_port_1_app_3_OnOffStateTrace.dat");

  generate2DGnuPlotFromDatFile(dirToSave + "/port_0_queue_0_PacketsInQueueTrace.dat", "High");
  generate2DGnuPlotFromDatFile(dirToSave + "/port_0_queue_1_PacketsInQueueTrace.dat", "Low");

  // Simulator::Cancel();
  Simulator::Destroy ();
  NS_LOG_INFO ("Stop simulation");
  
  return 0;
}

