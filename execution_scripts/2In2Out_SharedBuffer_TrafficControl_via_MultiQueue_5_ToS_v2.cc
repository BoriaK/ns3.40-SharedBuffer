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
 * Basic Topology, with 2 ports p0 and p1 with 5 client servers [t0,...t5] on each port, a switch S, 2 Tx ports connected to 5 
 * recievers [r0,...,r5] each
 * this design is based on DumbellTopoplogy model.
 * in this version all the NetDevices are created first and the TrafficControllHelper is installed on them simultanoiusly
 *
 *     t0__      _____      __r0
 *     t1__|    |     |    |__r1
 *     t2__|--p0|     |p0--|__r2
 *     t3__|    |     |    |__r3
 *     t4__|    |     |    |__r4
 *              |  S  |
 *     t0__     |     |     __r0
 *     t1__|    |     |    |__r1
 *     t2__|--p1|     |p1--|__r2
 *     t3__|    |     |    |__r3
 *     t4__|    |_____|    |__r4
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
#include <sstream>
#include <filesystem>

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
#include "ns3/object-factory.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/tcp-socket-state.h"
#include "ns3/tcp-header.h"

// There are 2 servers connecting to a leaf switch
#define SERVER_COUNT 2
#define SWITCH_COUNT 1
#define RECIEVER_COUNT 2

#define SWITCH_RECIEVER_CAPACITY  500*1e3       // Leaf-Spine Capacity 500[Kbps]/queue/port
#define SERVER_SWITCH_CAPACITY 5*1e6            // Serever-Leaf Capacity 5[Mbps]/queue/port
#define LINK_LATENCY MicroSeconds(20)           // each link latency 20 MicroSeconds 
#define BUFFER_SIZE 500                         // Shared Buffer Size for a single queue/port. 500[Packets]

// The simulation starting and ending time
#define START_TIME 0.0
#define DURATION_TIME 2
#define END_TIME 60

// The flow port range, each flow will be assigned a random port number within this range
#define SERV_PORT_P0 50000
#define SERV_PORT_P1 50001
#define SERV_PORT_P2 50002
#define SERV_PORT_P3 50003
#define SERV_PORT_P4 50004
 
#define PACKET_SIZE 1024 // 1024 [bytes]

using namespace ns3;

// Function to get workspace root directory
std::string GetWorkspaceRoot() {
    // Try to get current working directory
    char* cwd = getcwd(nullptr, 0);
    if (cwd == nullptr) {
        // Fallback to relative path if getcwd fails
        return "./Trace_Plots/2In2Out/";
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
            return wsRoot + "/Trace_Plots/2In2Out/";
        }
        
        // Go up one directory level
        size_t lastSlash = wsRoot.find_last_of('/');
        if (lastSlash == std::string::npos || lastSlash == 0) {
            break;
        }
        wsRoot = wsRoot.substr(0, lastSlash);
    }
    
    // If we couldn't find the workspace root, use current directory
    return workspaceDir + "/Trace_Plots/2In2Out/";
}

std::string dir = GetWorkspaceRoot();
std::string traffic_control_type = "SharedBuffer_DT"; // "SharedBuffer_DT"/"SharedBuffer_FB"
std::string implementation = "via_MultiQueues/5_ToS";  // "via_NetDevices"/"via_FIFO_QueueDiscs"/"via_MultiQueues"
std::string usedAlgorythm;  // "DT"/"FB"
std::string onOffTrafficMode = "Constant"; // "Constant"/"Uniform"/"Normal"/"Exponential"
// per-run output directory (set in main)
std::string runDir = "";
// for OnOff Aplications
uint32_t dataRate = 2; // [Mbps] data generation rate for a single OnOff application (increased to provoke congestion)
// time interval values
double_t trafficGenDuration = DURATION_TIME; // [sec] initilize for a single OnOff segment

// counter for total OnOff Tx packets (incremented by trace callback)
uint64_t g_totalOnOffTxPackets = 0;
// counter for total OnOff Tx bytes (incremented by trace callback)
uint64_t g_totalOnOffTxBytes = 0;

uint32_t prev = 0;
Time prevTime = Seconds (0);

NS_LOG_COMPONENT_DEFINE ("2In2Out");

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
  std::ofstream tcpisq (dir + traffic_control_type + "/" + implementation + "/TrafficControlPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  tcpisq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tcpisq.close ();
  
  // std::cout << "PacketsInSharedBuffer: " << newValue << std::endl;
}

void
TrafficControlHighPriorityPacketsInSharedQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream tchppisq (dir + traffic_control_type + "/" + implementation + "/TrafficControlHighPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  tchppisq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tchppisq.close ();
  
  // std::cout << "HighPriorityPacketsInSharedBuffer: " << newValue << std::endl;
}

void
TrafficControlLowPriorityPacketsInSharedQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream tclppisq (dir + traffic_control_type + "/" + implementation + "/TrafficControlLowPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  tclppisq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tclppisq.close ();
  
  // std::cout << "LowPriorityPacketsInSharedBuffer: " << newValue << std::endl;
}

// Trace the Threshold Value for High Priority packets in the Shared Queue
void
TrafficControlThresholdHighTrace (size_t index, float_t oldValue, float_t newValue)  // added by me, to monitor Threshold
{
  std::ofstream tchpthr (dir + traffic_control_type + "/" + implementation + "/TrafficControlHighPriorityQueueThreshold_" + ToString(index) + ".dat", std::ios::out | std::ios::app);
  tchpthr << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tchpthr.close ();

  // std::cout << "HighPriorityQueueThreshold on port: " << index << " is: " << newValue << " packets " << std::endl;
}

// Trace the Threshold Value for Low Priority packets in the Shared Queue
void
TrafficControlThresholdLowTrace (size_t index, float_t oldValue, float_t newValue)  // added by me, to monitor Threshold
{
  std::ofstream tclpthr (dir + traffic_control_type + "/" + implementation + "/TrafficControlLowPriorityQueueThreshold_" + ToString(index) + ".dat", std::ios::out | std::ios::app);
  tclpthr << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tclpthr.close ();
  
  // std::cout << "LowPriorityQueueThreshold on port: " << index << " is: " << newValue << " packets " << std::endl;
}

// void DroppedPacketHandler(std::string context, Ptr<const Packet> packet) 
// {
//     std::cout << "Packet dropped by traffic control layer: " << packet->ToString() << std::endl;
// }
//////////////////////////////////////////////////////

void
QueueDiscPacketsInQueueTrace (size_t portIndex, size_t queueIndex, uint32_t oldValue, uint32_t newValue)
{
  std::ofstream qdpiq (dir + traffic_control_type + "/" + implementation + "/port_" + ToString(portIndex) + "_queue_" + ToString(queueIndex) + "_PacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  qdpiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  qdpiq.close ();
  
  // std::cout << "QueueDiscPacketsInPort " << portIndex << " Queue " << queueIndex << ": " << newValue << std::endl;
}

// Traffic Control Layer drop trace
void
TrafficControlDropTrace (Ptr<const Packet> packet)
{
  std::ofstream f (dir + traffic_control_type + "/" + implementation + "/TrafficControl_Drop.dat", std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << (packet ? packet->GetSize() : 0) << std::endl;
  f.close();
}

// TCP ECN/CE/CWR tracing
void
TcpEcnTrace (SequenceNumber32 oldSeq, SequenceNumber32 newSeq)
{
  std::ofstream f (dir + traffic_control_type + "/" + implementation + "/TcpEcnTrace.dat", std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << newSeq.GetValue() << std::endl;
  f.close();
}

// TCP RTO trace callback (free function so MakeCallback can deduce)
// signature uses traced-value old/new pair
void
TcpRtoTraceCallback (Time oldRto, Time newRto)
{
  std::string out = runDir.empty() ? (dir + traffic_control_type + "/" + implementation + "/TcpRtoTrace.dat") : (runDir + "/TcpRtoTrace.dat");
  std::ofstream f(out, std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << newRto.GetSeconds() << std::endl;
  f.close();
}

// TCP Retransmits trace callback (free function)
// signature uses traced-value old/new pair
void
TcpRetransmitsCallback (uint32_t oldRetrans, uint32_t newRetrans)
{
  std::string out = runDir.empty() ? (dir + traffic_control_type + "/" + implementation + "/TcpRetransmits.dat") : (runDir + "/TcpRetransmits.dat");
  std::ofstream f(out, std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << newRetrans << std::endl;
  f.close();
}

// TCP BytesInFlight trace callback
void
TcpBytesInFlightCallback (uint32_t oldBif, uint32_t newBif)
{
  std::string out = runDir.empty() ? (dir + traffic_control_type + "/" + implementation + "/TcpBytesInFlight.dat") : (runDir + "/TcpBytesInFlight.dat");
  std::ofstream f(out, std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << newBif << std::endl;
  f.close();
}

// TCP EcnState trace callback (enum/int)
void
TcpEcnStateCallback (TcpSocketState::EcnState_t oldState, TcpSocketState::EcnState_t newState)
{
  std::string out = runDir.empty() ? (dir + traffic_control_type + "/" + implementation + "/TcpEcnState.dat") : (runDir + "/TcpEcnState.dat");
  std::ofstream f(out, std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << static_cast<uint32_t>(newState) << std::endl;
  f.close();
}

// TCP receiver window (RWND) trace callback
void
TcpRwndTraceCallback (uint32_t oldVal, uint32_t newVal)
{
  std::string out = runDir.empty() ? (dir + traffic_control_type + "/" + implementation + "/TcpRWND.dat") : (runDir + "/TcpRWND.dat");
  std::ofstream f(out, std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
  f.close();
}

// TCP advertised window (AdvWND) trace callback
void
TcpAdvWndTraceCallback (uint32_t oldVal, uint32_t newVal)
{
  std::string out = runDir.empty() ? (dir + traffic_control_type + "/" + implementation + "/TcpAdvWND.dat") : (runDir + "/TcpAdvWND.dat");
  std::ofstream f(out, std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
  f.close();
}

// TCP slow start threshold (ssthresh) trace callback
void
TcpSsthreshTraceCallback (uint32_t oldVal, uint32_t newVal)
{
  std::string out = runDir.empty() ? (dir + traffic_control_type + "/" + implementation + "/TcpSsthresh.dat") : (runDir + "/TcpSsthresh.dat");
  std::ofstream f(out, std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
  f.close();
}

// TCP Tx/Rx traced callback: signature (Ptr<const Packet>, TcpHeader, Ptr<const TcpSocketBase>)
void
TcpTxRxCallback (Ptr<const Packet> p, const TcpHeader& header, Ptr<const TcpSocketBase> sock)
{
  std::string out = runDir.empty() ? (dir + traffic_control_type + "/" + implementation + "/TcpTxRx.dat") : (runDir + "/TcpTxRx.dat");
  std::ofstream f(out, std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << (p ? p->GetSize() : 0) << " " << header.GetSequenceNumber().GetValue() << std::endl;
  f.close();
}

// TCP segment sent tracer: logs time, nodeId, sequence number, and packet size
void
TcpSegmentSentTrace (Ptr<const Packet> p, const TcpHeader& header, Ptr<const TcpSocketBase> sock)
{
  std::string out = runDir.empty() ? (dir + traffic_control_type + "/" + implementation + "/TcpSegmentsOut.dat") : (runDir + "/TcpSegmentsOut.dat");
  std::ofstream f(out, std::ios::out | std::ios::app);
  if (!f.is_open()) return;
  static bool printed = false;
  uint32_t seq = header.GetSequenceNumber().GetValue();
  uint32_t nodeId = 0;
  if (sock && sock->GetNode())
  {
    nodeId = sock->GetNode()->GetId();
  }
  if (!printed)
  {
    std::cout << "TcpSegmentSentTrace attached - logging segments to: " << out << std::endl;
    printed = true;
  }
  // write: time nodeId seq size
  f << Simulator::Now().GetSeconds() << " " << nodeId << " " << seq << " " << (p ? p->GetSize() : 0) << std::endl;
  f.close();
}

// OnOff applications Tx packet callback: increment global counter and log cumulative count
void
OnOffTxPacketCallback (Ptr<const Packet> p)
{
  // increment global counter
  ++g_totalOnOffTxPackets;
  // accumulate bytes sent by the OnOff applications
  uint32_t pktSize = (p ? p->GetSize() : 0);
  g_totalOnOffTxBytes += pktSize;

  // append timestamp + cumulative count to a per-run dat file so we can plot if desired
  std::string out = runDir.empty() ? (dir + traffic_control_type + "/" + implementation + "/OnOff_TxPackets.dat") : (runDir + "/OnOff_TxPackets.dat");
  std::ofstream f(out, std::ios::out | std::ios::app);
  // write: time cumulative_packets cumulative_bytes
  f << Simulator::Now().GetSeconds() << " " << g_totalOnOffTxPackets << " " << g_totalOnOffTxBytes << std::endl;
  f.close();
}

// Forward declaration for cwnd trace used later when attaching to sockets
void TcpCwndTrace (uint32_t oldCwnd, uint32_t newCwnd);

// Attach congestion-window traces directly to sockets created by applications.
void
AttachTcpSocketTraces(const ApplicationContainer& apps)
{
  for (size_t i = 0; i < apps.GetN(); ++i)
  {
    Ptr<Application> app = apps.Get(i);
    Ptr<PrioOnOffApplication> poa = DynamicCast<PrioOnOffApplication>(app);
    if (poa)
    {
      Ptr<Socket> s = poa->GetSocket();
      if (s)
      {
        Ptr<TcpSocketBase> ts = DynamicCast<TcpSocketBase>(s);
        if (ts)
        {
          ts->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&TcpCwndTrace));
          ts->TraceConnectWithoutContext("CongestionWindowInflated", MakeCallback(&TcpCwndTrace));
          // Attach retransmit and RTO traces
          ts->TraceConnectWithoutContext("RTO", MakeCallback(&TcpRtoTraceCallback));
          ts->TraceConnectWithoutContext("Retransmits", MakeCallback(&TcpRetransmitsCallback));
          // Attach additional sender-side traces
          ts->TraceConnectWithoutContext("BytesInFlight", MakeCallback(&TcpBytesInFlightCallback));
          ts->TraceConnectWithoutContext("EcnState", MakeCallback(&TcpEcnStateCallback));
          ts->TraceConnectWithoutContext("RWND", MakeCallback(&TcpRwndTraceCallback));
          ts->TraceConnectWithoutContext("AdvWND", MakeCallback(&TcpAdvWndTraceCallback));
          ts->TraceConnectWithoutContext("SlowStartThreshold", MakeCallback(&TcpSsthreshTraceCallback));
          ts->TraceConnectWithoutContext("Tx", MakeCallback(&TcpTxRxCallback));
          ts->TraceConnectWithoutContext("Rx", MakeCallback(&TcpTxRxCallback));
          // Also log per-segment sends (time, seq, size)
          ts->TraceConnectWithoutContext("Tx", MakeCallback(&TcpSegmentSentTrace));
        }
      }
    }
  }
}

void
HighPriorityQueueDiscPacketsInQueueTrace (size_t portIndex, size_t queueIndex, uint32_t oldValue, uint32_t newValue)
{
  std::ofstream hpqdpiq (dir + traffic_control_type + "/" + implementation + "/port_" + ToString(portIndex) + "_queue_" + ToString(queueIndex) + "_HighPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  hpqdpiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  hpqdpiq.close ();
  
  // std::cout << "HighPriorityQueueDiscPacketsInPort " << portIndex << " Queue " << queueIndex << ": " << newValue << std::endl;
}

void
LowPriorityQueueDiscPacketsInQueueTrace (size_t portIndex, size_t queueIndex, uint32_t oldValue, uint32_t newValue)
{
  std::ofstream lpqdpiq (dir + traffic_control_type + "/" + implementation + "/port_" + ToString(portIndex) + "_queue_" + ToString(queueIndex) + "_LowPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  lpqdpiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  lpqdpiq.close ();
  
  // std::cout << "LowPriorityQueueDiscPacketsInPort " << portIndex << " Queue " << queueIndex << ": " << newValue << std::endl;
}

// to monitor the OnOff state of a single OnOff Application in real time
void
checkOnOffState(Ptr<Application>& app, size_t portIndex, size_t appIndex, double maxDuration, double startTime)
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
    Simulator::Schedule(Seconds(0.001), &checkOnOffState, app, portIndex, appIndex, maxDuration, startTime);

    // save to .dat file
    std::ofstream fPlotOnOffState(std::stringstream(dir + traffic_control_type + "/" + implementation + 
    "/" + "port_" + IntToString(portIndex) + "_app_" + IntToString(appIndex) + "_OnOffStateTrace.dat").str(),
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
    std::ofstream fPlotOnOffTrafficState(std::stringstream(dir + traffic_control_type + "/" + implementation + "/generatedOnOffTrafficTrace.dat").str(),
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
  dataset.SetStyle(Gnuplot2dDataset::LINES);
  
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

// TCP congestion window trace
void
TcpCwndTrace (uint32_t oldCwnd, uint32_t newCwnd)
{
  std::string out = runDir.empty() ? (dir + traffic_control_type + "/" + implementation + "/TxCongestionWindowChange.dat") : (runDir + "/TxCongestionWindowChange.dat");
  std::ofstream f (out, std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
  f.close();
}

// forward declare so functions defined earlier can reference it
void TcpCwndTrace (uint32_t oldCwnd, uint32_t newCwnd);

// QueueDisc Drop/Mark trace callbacks
void
QueueDiscDropTrace (Ptr<const QueueDiscItem> item)
{
  std::ofstream f (dir + traffic_control_type + "/" + implementation + "/QueueDiscDropTrace.dat", std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << item->GetPacket()->GetSize() << std::endl;
  f.close();
}

void
QueueDiscMarkTrace (Ptr<const QueueDiscItem> item)
{
  std::ofstream f (dir + traffic_control_type + "/" + implementation + "/QueueDiscMarkTrace.dat", std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << item->GetPacket()->GetSize() << std::endl;
  f.close();
}

// overloads that accept a reason string (some traces provide a reason param)
void
QueueDiscDropTrace (Ptr<const QueueDiscItem> item, const char* reason)
{
  std::ofstream f (dir + traffic_control_type + "/" + implementation + "/QueueDiscDropTrace.dat", std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << item->GetPacket()->GetSize() << " " << (reason ? reason : "") << std::endl;
  f.close();
}

void
QueueDiscMarkTrace (Ptr<const QueueDiscItem> item, const char* reason)
{
  std::ofstream f (dir + traffic_control_type + "/" + implementation + "/QueueDiscMarkTrace.dat", std::ios::out | std::ios::app);
  f << Simulator::Now().GetSeconds() << " " << item->GetPacket()->GetSize() << " " << (reason ? reason : "") << std::endl;
  f.close();
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("2In2Out", LOG_LEVEL_INFO);

  // Change the current working directory to the desired path
  std::string currentPath = "/home/boris/workspace/ns3.40-SharedBuffer";
  // Set the current working directory
  system (("cd " + currentPath).c_str ());

  double_t miceElephantProb = 0.2;
  double_t alpha_high = 15;
  double_t alpha_low = 5;

  std::string applicationType = "prioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"prioOnOff"
  // Command line parameters parsing
  std::string transportProt = "TCP"; // "UDP"/"TCP"
  std::string cc = "ns3::TcpNoCongestion"; // if transportProt = "TCP", chose: "ns3::TcpNewReno"/"ns3::TcpNoCongestion"
  std::string socketType;
  std::string queue_capacity;

  uint64_t serverSwitchCapacity = 5 * SERVER_SWITCH_CAPACITY;
  uint64_t switchRecieverCapacity = 5 * SWITCH_RECIEVER_CAPACITY;

  // Create a new directory to store the output of the program
  // dir = "./Trace_Plots/2In2Out/";
  std::string dirToSave = dir + traffic_control_type + "/" + implementation + "/" + onOffTrafficMode + "/" + DoubleToString(miceElephantProb) + "/" + DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) + "/" + transportProt;
  if (!((std::filesystem::exists(dirToSave)) && (std::filesystem::is_directory(dirToSave))))
  {
    system (("mkdir -p " + dirToSave).c_str ());
  }

  // set global runDir so trace callbacks write into this per-run directory
  runDir = dirToSave;


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
  cmd.AddValue ("cc", "TCP congestion control type (e.g. ns3::TcpNewReno or ns3::TcpNoCongestion)", cc);
  cmd.AddValue ("flowletTimeout", "flowlet timeout", flowletTimeout);
  cmd.Parse (argc, argv);

  if (traffic_control_type.compare("SharedBuffer_DT") == 0)
  {
    usedAlgorythm = "DT";
  }
  else if (traffic_control_type.compare("SharedBuffer_FB") == 0)
  {
    usedAlgorythm = "FB";
  }

  // Application type dependent parameters
  if (applicationType.compare("standardClient") == 0 || applicationType.compare("prioClient") == 0)
  {
    queue_capacity = "20p"; // B, the total space on the buffer.
  }
  else
  {
    queue_capacity = ToString(5 * BUFFER_SIZE) + "p"; // B, the total space on the buffer [packets]
  }

  // client type dependant parameters:
  if (transportProt.compare ("TCP") == 0)
  {
  // Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno")); // "ns3::TcpNewReno"/"ns3::TcpNoCongestion"
    // Select the congestion control via TcpL4Protocol::SocketType (can be overridden with --cc)
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (cc));
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
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(PACKET_SIZE + 40)); // add 40 bytes for TCP/IP headers
  Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1000));             // User-requested large initial window
  
  // 4. Enable TCP timestamps as requested (helps with RTT measurement and PAWS)
  Config::SetDefault("ns3::TcpSocketBase::Timestamp", BooleanValue(true));        // Enable timestamps for better RTT estimation
  Config::SetDefault("ns3::TcpSocketBase::WindowScaling", BooleanValue(true));    // Keep window scaling (needed for large buffers)
  
  // === Anti-spurious retransmission optimizations ===
  
  // 5. Conservative delayed ACK to reduce reverse traffic and potential ACK compression
  Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(500))); // Longer delay to avoid ACK storms
  Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(4));               // Wait for more segments before ACKing
  
  // 6. Enable advanced recovery mechanisms
  Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(true));             // Enable SACK for better recovery
  Config::SetDefault("ns3::TcpSocketBase::LimitedTransmit", BooleanValue(false)); // Disable limited transmit to reduce spurious sends
  
  // 7. Conservative data retry settings  
  Config::SetDefault("ns3::TcpSocket::DataRetries", UintegerValue(12));           // More retries before giving up
  
  // 8. Additional anti-spurious optimizations
  Config::SetDefault("ns3::TcpSocket::TcpNoDelay", BooleanValue(false));          // Enable Nagle's algorithm to reduce small packets
  Config::SetDefault("ns3::TcpSocket::PersistTimeout", TimeValue(Seconds(10))); // Longer persist timeout for window probes
  
  // 9. Connection establishment timeout (reduce SYN retransmissions) 
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
  Names::Add("Router", router.Get(0));  // Add a Name to the router node
  NodeContainer servers;
  servers.Create (SERVER_COUNT);

  PointToPointHelper n2s, s2r;
  NS_LOG_INFO ("Configuring channels for all the Nodes");
  // Setting servers
  n2s.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (serverSwitchCapacity)));
  n2s.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
  n2s.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));  // set basic queues to 1 packet
  // setting routers
  s2r.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (switchRecieverCapacity)));
  s2r.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
  s2r.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));  // set basic queues to 1 packet

  NS_LOG_INFO ("Create NetDevices");
  NetDeviceContainer serverDevices;
  NetDeviceContainer switchDevicesIn;
  NetDeviceContainer switchDevicesOut;
  NetDeviceContainer recieverDevices;

  NS_LOG_INFO ("Install NetDevices on all Nodes");
  NS_LOG_INFO ("Configuring Servers");
  for (int i = 0; i < SERVER_COUNT; i++)
  {
    NS_LOG_INFO ("Server " << i << " is connected to switch");

    NetDeviceContainer tempNetDevice = n2s.Install (servers.Get (i), router.Get(0));
    serverDevices.Add(tempNetDevice.Get(0));
    switchDevicesIn.Add(tempNetDevice.Get(1));
  }
  

  NS_LOG_INFO ("Configuring switches");


  for (int i = 0; i < RECIEVER_COUNT; i++)
  {
    NetDeviceContainer tempNetDevice = s2r.Install (router.Get(0), recievers.Get (i));
    switchDevicesOut.Add(tempNetDevice.Get(0));
    recieverDevices.Add(tempNetDevice.Get(1));

    NS_LOG_INFO ("Switch is connected to Reciever " << i << " at capacity: " << switchRecieverCapacity);     
  }

  for (size_t i = 0; i < switchDevicesOut.GetN(); i++) // add a "name" to the "switchDeviceOut" NetDevices
  {     
    Names::Add("switchDeviceOut" + IntToString(i), switchDevicesOut.Get(i));  // Add a Name to the switch net-devices
  }

  // Now add ip/tcp stack to all nodes. this is a VERY IMPORTANT COMPONENT!!!!
  NS_LOG_INFO ("Install Internet stacks");
  InternetStackHelper internet;
  internet.InstallAll ();

  NS_LOG_INFO ("Install QueueDisc");

  TrafficControlHelper tch;
  // priomap with low priority for class "0" and high priority for rest of the 15 classes (1-15). Technically not nesessary for RoundRobinPrioQueue
  std::array<uint16_t, 16> prioArray = {4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  
  TosMap tosMap = TosMap{prioArray};
  uint16_t handle = tch.SetRootQueueDisc("ns3::RoundRobinTosQueueDisc", "TosMap", TosMapValue(tosMap));

  TrafficControlHelper::ClassIdList cid = tch.AddQueueDiscClasses(handle, 5, "ns3::QueueDiscClass");
  tch.AddChildQueueDisc(handle, cid[0], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[0] is band "0" - the Highest Priority band
  tch.AddChildQueueDisc(handle, cid[1], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[1] is < cid[0]
  tch.AddChildQueueDisc(handle, cid[2], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[2] is < cid[1]
  tch.AddChildQueueDisc(handle, cid[3], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[3] is < cid[2]
  tch.AddChildQueueDisc(handle, cid[4], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[4] is < cid[3]

  // in this option we installed TCH on switchDevicesOut. to send data from switch to reciever
  QueueDiscContainer qdiscs = tch.Install (switchDevicesOut);  

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

  // monitor the packets in the Shared Buffer in Traffic Control Layer:
  tc->TraceConnectWithoutContext("PacketsInQueue", MakeCallback (&TrafficControlPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("HighPriorityPacketsInQueue", MakeCallback (&TrafficControlHighPriorityPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("LowPriorityPacketsInQueue", MakeCallback (&TrafficControlLowPriorityPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_High_0", MakeBoundCallback (&TrafficControlThresholdHighTrace, 0));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_Low_0", MakeBoundCallback (&TrafficControlThresholdLowTrace, 0));  
  tc->TraceConnectWithoutContext("EnqueueingThreshold_High_1", MakeBoundCallback (&TrafficControlThresholdHighTrace, 1));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_Low_1", MakeBoundCallback (&TrafficControlThresholdLowTrace, 1));
  // Also trace drops from TrafficControlLayer (TcDrop)
  tc->TraceConnectWithoutContext("TcDrop", MakeCallback(&TrafficControlDropTrace));

  //////////////Monitor data from q-disc//////////////////////////////////////////
  for (size_t i = 0; i < RECIEVER_COUNT; i++)  // over all ports
  {
    for (size_t j = 0; j < qdiscs.Get (i)->GetNQueueDiscClasses(); j++)  // over all the queues per port
    {
      Ptr<QueueDisc> queue = qdiscs.Get (i)->GetQueueDiscClass(j)->GetQueueDisc();
      queue->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&QueueDiscPacketsInQueueTrace, i, j));
      // queue->TraceConnectWithoutContext ("HighPriorityPacketsInQueue", MakeBoundCallback (&HighPriorityQueueDiscPacketsInQueueTrace, i, j)); 
      // queue->TraceConnectWithoutContext ("LowPriorityPacketsInQueue", MakeBoundCallback (&LowPriorityQueueDiscPacketsInQueueTrace, i, j)); 
      // also trace Drop and Mark events (use matching signatures)
      queue->TraceConnectWithoutContext("Drop", MakeCallback(static_cast<void (*)(Ptr<const QueueDiscItem>)>(&QueueDiscDropTrace)));
      queue->TraceConnectWithoutContext("Mark", MakeCallback(static_cast<void (*)(Ptr<const QueueDiscItem>, const char*)>(&QueueDiscMarkTrace)));
    }
  }
  ////////////////////////////////////////////////////////////////////////////////

  NS_LOG_INFO ("Setup IPv4 Addresses");

  ns3::Ipv4AddressHelper server2switchIPs =
      ns3::Ipv4AddressHelper("10.1.0.0", "255.255.255.0");
  ns3::Ipv4AddressHelper switch2recieverIPs =
      ns3::Ipv4AddressHelper("10.2.0.0", "255.255.255.0");


  Ipv4InterfaceContainer serverIFs;
  Ipv4InterfaceContainer switchInIFs;
  Ipv4InterfaceContainer switchOutIFs;
  Ipv4InterfaceContainer recieverIFs;



  NS_LOG_INFO ("Install IPv4 addresses on all NetDevices");
  
  NS_LOG_INFO ("Configuring servers");

  for (int i = 0; i < SERVER_COUNT; i++)
  {
    NetDeviceContainer tempNetDevice;
    tempNetDevice.Add(serverDevices.Get(i));
    tempNetDevice.Add(switchDevicesIn.Get(i));
    Ipv4InterfaceContainer ifcServers = server2switchIPs.Assign(tempNetDevice);
    serverIFs.Add(ifcServers.Get(0));
    switchInIFs.Add(ifcServers.Get(1));

    server2switchIPs.NewNetwork ();
  }

  NS_LOG_INFO ("Configuring switch");

  for (int i = 0; i < RECIEVER_COUNT; i++)
  {
      NetDeviceContainer tempNetDevice;
      tempNetDevice.Add(switchDevicesOut.Get(i));
      tempNetDevice.Add(recieverDevices.Get(i));
      Ipv4InterfaceContainer ifcSwitch = switch2recieverIPs.Assign(tempNetDevice);
      switchOutIFs.Add(ifcSwitch.Get(0));
      recieverIFs.Add(ifcSwitch.Get(1));

      switch2recieverIPs.NewNetwork ();    
  }


  // and setup ip routing tables to get total ip-level connectivity.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Sockets, Applications and Sinks");

  std::vector<uint32_t> serverPort_vector;
  for (size_t i = 0; i < 5; i++)
  {
    serverPort_vector.push_back(50000 + i);
  }

  // create a vector of IP_ToS_Priorities: P0>P1>...>P4
  std::vector<uint32_t> ipTos_vector;
  uint32_t ipTos_P0 = 0x10; 
  ipTos_vector.push_back(ipTos_P0);
  uint32_t ipTos_P1 = 0x06; 
  ipTos_vector.push_back(ipTos_P1);
  uint32_t ipTos_P2 = 0x04; 
  ipTos_vector.push_back(ipTos_P2);
  uint32_t ipTos_P3 = 0x02; 
  ipTos_vector.push_back(ipTos_P3);
  uint32_t ipTos_P4 = 0x00; 
  ipTos_vector.push_back(ipTos_P4);

  
  ApplicationContainer sinkApps, sourceApps;

  // time interval values for OnOff Aplications
  double_t miceOnTime = 0.05; // [sec]
  double_t elephantOnTime = 0.5; // [sec]
  double_t miceOffTime = 0.01; // [sec]
  double_t elephantOffTime = 0.1; // [sec]
  // for RNG:
  double_t miceOnTimeMax = 2 * miceOnTime; // [sec]
  double_t elephantOnTimeMax = 2 * elephantOnTime; // [sec]
  double_t miceOffTimeMax = 2 * miceOffTime; // [sec]
  double_t elephantOffTimeMax = 2 * elephantOffTime; // [sec]

  double_t numHpMachines = miceElephantProb * 10; // number of OnOff machines that generate High priority traffic
  // int Num_E = 10 - numHpMachines; // number of OnOff machines that generate Low priority traffic
  bool unequalNum = false; // a flag that's raised if the number of High/Low priority OnOff applications created is diffeent for each Port
  if ((int(miceElephantProb * 10) % 2) != 0)
  {
    unequalNum = true;
  }
  
  for (size_t i = 0; i < 2; i++)
  {
    int serverIndex = i;
    int recieverIndex = i;
    // create sockets
    ns3::Ptr<ns3::Socket> sockptr;

    if (transportProt.compare("TCP") == 0) 
    {
      // setup source socket
      sockptr =
          ns3::Socket::CreateSocket(servers.Get(serverIndex),
                  ns3::TcpSocketFactory::GetTypeId());
      // sockptr->SetIpTos();          
      // ns3::Ptr<ns3::TcpSocket> tcpsockptr =
      //     ns3::DynamicCast<ns3::TcpSocket> (sockptr);
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
    } 
    else 
    {
      std::cerr << "unknown transport type: " <<
          transportProt << std::endl;
      exit(1);
    }
    
    // create InetSocketAddresses and PrioOnOffHelper vectors
    std::vector<InetSocketAddress> socketAddress_vector;
    std::vector<PrioOnOffHelper> clientHelpers_vector;
    for (size_t j = 0; j < 5; j++)
    {
      InetSocketAddress tempSocketAddress = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), serverPort_vector[j]);
      tempSocketAddress.SetTos(ipTos_vector[j]); 
      socketAddress_vector.push_back(tempSocketAddress);
      PrioOnOffHelper tempClientHelper(socketType, socketAddress_vector[j]);
      clientHelpers_vector.push_back(tempClientHelper);
    }

    NS_LOG_INFO ("Configure OnOff Applications");
    // create and install Client apps:    
    if (applicationType.compare("prioOnOff") == 0) 
    {
      for (size_t j = 0; j < 5; j++) 
      {
        clientHelpers_vector[j].SetAttribute ("Remote", AddressValue (socketAddress_vector[j]));
        if (unequalNum) 
        {
          // in case the number of High/Low priority OnOff applications is not equally devidable by 2, 
          // divide the High/Low priority OnOff applications between the 2 servers such that:
          // 1st server recives the higher number of High priority and lower number of Low priority OnOff applications
          // and the 2nd server recives lower number of High priority and higher number of Low priority OnOff applications
          if (i == 0 && j < int(ceil(numHpMachines/2)) || i == 1 && j < int(floor(numHpMachines/2)))
          {
            if (onOffTrafficMode.compare("Constant") == 0)
            { 
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
            }
            else if (onOffTrafficMode.compare("Uniform") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
            }
            else if (onOffTrafficMode.compare("Normal") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
            }
            else if (onOffTrafficMode.compare("Exponential") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ExponentialRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(trafficGenDuration) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(trafficGenDuration) +"]"));
            }
            else 
            {
              std::cerr << "unknown OnOffMode type: " << onOffTrafficMode << std::endl;
              exit(1);
            }
            clientHelpers_vector[j].SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
          }
          else
          {
            if (onOffTrafficMode.compare("Constant") == 0)
            { 
            clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
            clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
            }
            else if (onOffTrafficMode.compare("Uniform") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(elephantOnTimeMax) +"]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
            }
            else if (onOffTrafficMode.compare("Normal") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(elephantOffTime) +"]"));
            }
            else if (onOffTrafficMode.compare("Exponential") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ExponentialRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(trafficGenDuration) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(trafficGenDuration) +"]"));
            }
            else 
            {
              std::cerr << "unknown OnOffMode type: " << onOffTrafficMode << std::endl;
              exit(1);
            }
            clientHelpers_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low 
          }
        }
        else
        {
          if (j < int(numHpMachines/2)) // create OnOff machines that generate High priority traffic
          {
            if (onOffTrafficMode.compare("Constant") == 0)
            { 
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
            }
            else if (onOffTrafficMode.compare("Uniform") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
            }
            else if (onOffTrafficMode.compare("Normal") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
            }
            else if (onOffTrafficMode.compare("Exponential") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ExponentialRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(trafficGenDuration) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(trafficGenDuration) +"]"));
            }
            else 
            {
              std::cerr << "unknown OnOffMode type: " << onOffTrafficMode << std::endl;
              exit(1);
            }
            clientHelpers_vector[j].SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
          }
          else // create OnOff machines that generate Low priority traffic
          {
            if (onOffTrafficMode.compare("Constant") == 0)
            { 
            clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
            clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
            }
            else if (onOffTrafficMode.compare("Uniform") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(elephantOnTimeMax) +"]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
            }
            else if (onOffTrafficMode.compare("Normal") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(elephantOffTime) +"]"));
            }
            else if (onOffTrafficMode.compare("Exponential") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ExponentialRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(trafficGenDuration) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(trafficGenDuration) +"]"));
            }
            else 
            {
              std::cerr << "unknown OnOffMode type: " << onOffTrafficMode << std::endl;
              exit(1);
            }
            clientHelpers_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low  
          }
        }
        clientHelpers_vector[j].SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelpers_vector[j].SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        clientHelpers_vector[j].SetAttribute ("ApplicationToS", UintegerValue (ipTos_vector[j])); // set the IP ToS value for the application
        // clientHelpers_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelpers_vector[j].SetAttribute("MiceElephantProbability", StringValue (DoubleToString(miceElephantProb)));
        clientHelpers_vector[j].SetAttribute("StreamIndex", UintegerValue (1 + 2*(i + j))); // assign a stream for RNG for each OnOff application instanse
    sourceApps.Add(clientHelpers_vector[j].Install (servers.Get(serverIndex)));

    // attach OnOff Tx packet counter to this newly installed application
    // schedule a short delay to ensure the app object exists
    Simulator::Schedule(Seconds(1.0), [&, j]() {
      Ptr<Application> app = sourceApps.Get(j);
      if (app)
      {
        Ptr<PrioOnOffApplication> poa = DynamicCast<PrioOnOffApplication>(app);
        if (poa)
        {
          poa->TraceConnectWithoutContext("Tx", MakeCallback(&OnOffTxPacketCallback));
        }
        else
        {
          // try generic CustomOnOffApplication
          Ptr<CustomOnOffApplication> coa = DynamicCast<CustomOnOffApplication>(app);
          if (coa)
          {
            coa->TraceConnectWithoutContext("Tx", MakeCallback(&OnOffTxPacketCallback));
          }
        }
      }
    });

    //monitor OnOff Traffic generated from the OnOff Applications:
    Simulator::Schedule (Seconds (1.0), &checkOnOffState, sourceApps.Get(j), i, j, 3.1, Simulator::Now().GetSeconds());
      }
    }
    else 
    {
      std::cerr << "unknown app type: " << applicationType << std::endl;
      exit(1);
    }
    
    NS_LOG_INFO ("Configure Packet Sinks");
    // setup sinks
    Address sinkLocalAddressP0 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P0));
    Address sinkLocalAddressP1 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P1));
    Address sinkLocalAddressP2 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P2));
    Address sinkLocalAddressP3 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P3));
    Address sinkLocalAddressP4 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P4));
    PacketSinkHelper sinkP0 (socketType, sinkLocalAddressP0); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    PacketSinkHelper sinkP1 (socketType, sinkLocalAddressP1); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    PacketSinkHelper sinkP2 (socketType, sinkLocalAddressP2); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    PacketSinkHelper sinkP3 (socketType, sinkLocalAddressP3); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    PacketSinkHelper sinkP4 (socketType, sinkLocalAddressP4); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    sinkApps.Add(sinkP0.Install (recievers.Get(recieverIndex)));
    sinkApps.Add(sinkP1.Install (recievers.Get(recieverIndex))); 
    sinkApps.Add(sinkP2.Install (recievers.Get(recieverIndex)));
    sinkApps.Add(sinkP3.Install (recievers.Get(recieverIndex)));
    sinkApps.Add(sinkP4.Install (recievers.Get(recieverIndex)));     
  }

  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds(1.0 + trafficGenDuration));

  // Attach per-socket TCP traces shortly after applications start so sockets are created
  // Schedule twice with small jitter to increase the chance of attaching to all sockets
  Simulator::Schedule(Seconds(1.01), &AttachTcpSocketTraces, sourceApps);
  Simulator::Schedule(Seconds(1.05), &AttachTcpSocketTraces, sourceApps);

  sinkApps.Start (Seconds (START_TIME));
  sinkApps.Stop (Seconds (END_TIME + 0.1));

  if (transportProt.compare("TCP") == 0) 
  {
    NS_LOG_INFO ("Configure TCP socket traces");
    // Connect TCP congestion window traces for each server node (SocketList/0)
    for (size_t i = 0; i < servers.GetN(); ++i)
    {
      std::ostringstream path;
      path << "/NodeList/" << servers.Get(i)->GetId() << "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow";
      Config::ConnectWithoutContext(path.str(), MakeCallback(&TcpCwndTrace));
    }
    // Also connect a wildcard path to catch congestion window changes for any socket
    // Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeCallback(&TcpCwndTrace));

    // Connect TCP ECN trace sources (EcnEchoSeq, EcnCeSeq, EcnCwrSeq) for diagnosis
    for (size_t i = 0; i < servers.GetN(); ++i)
    {
      std::ostringstream ecnPath;
      ecnPath << "/NodeList/" << servers.Get(i)->GetId() << "/$ns3::TcpL4Protocol/SocketList/0/EcnEchoSeq";
      Config::ConnectWithoutContext(ecnPath.str(), MakeCallback(&TcpEcnTrace));
      std::ostringstream ecnCePath;
      ecnCePath << "/NodeList/" << servers.Get(i)->GetId() << "/$ns3::TcpL4Protocol/SocketList/0/EcnCeSeq";
      Config::ConnectWithoutContext(ecnCePath.str(), MakeCallback(&TcpEcnTrace));
      std::ostringstream ecnCwrPath;
      ecnCwrPath << "/NodeList/" << servers.Get(i)->GetId() << "/$ns3::TcpL4Protocol/SocketList/0/EcnCwrSeq";
      Config::ConnectWithoutContext(ecnCwrPath.str(), MakeCallback(&TcpEcnTrace));
    }
  // Connect per-segment Tx trace for TCP sockets (log every segment sent)
  // NOTE: per-socket segment traces are attached in AttachTcpSocketTraces to avoid
  // duplicate logging that can occur with a wildcard Config connect.
  // (per-socket attachment scheduled below)
  }

  //monitor OnOff Traffic generated from all the OnOff Applications (combined):
  Simulator::Schedule (Seconds (1.0), &returnOnOffTraffic, sourceApps, 3.1, Simulator::Now().GetSeconds());

  NS_LOG_INFO ("Enabling flow monitor");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll(); 

  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (END_TIME + 10));
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl 
            << "Topology: 2In2Out" << std::endl;
  std::cout << "Queueing Algorithm: " + traffic_control_type << std::endl;
  std::cout << "Implementation Method: " + implementation << std::endl;
  std::cout << "Used D value: " + DoubleToString(miceElephantProb) << std::endl;
  std::cout << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
  std::cout << "Application: " + applicationType << std::endl;
  std::cout << "Transport Protocol: " + transportProt << std::endl;
  std::cout << "traffic Mode " + onOffTrafficMode + "Random" << std::endl;
  std::cout << "Traffic Duration: " + DoubleToString(trafficGenDuration) + " [Sec]" << std::endl;

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
  // a loop to sum the Tx/Rx Packets/Bytes from all nodes
  uint32_t statTxPackets = 0; 
  uint64_t statTxBytes = 0;
  uint32_t statRxPackets = 0; 
  uint64_t statRxBytes = 0;
  for (size_t i = 1; i <= stats.size(); i++)
  // stats indexing needs to start from 1
  {
    statTxPackets = statTxPackets + stats[i].txPackets;
    statTxBytes = statTxBytes + stats[i].txBytes;
    statRxPackets = statRxPackets + stats[i].rxPackets;
    statRxBytes = statRxBytes + stats[i].rxBytes;
  }

  std::cout << "  Tx Packets/Bytes:   " << statTxPackets
              << " / " << statTxBytes << std::endl;
  std::cout << "  Rx Packets/Bytes:   " << statRxPackets
              << " / " << statRxBytes << std::endl;

  uint32_t packetsDroppedByQueueDisc = 0;
  uint64_t bytesDroppedByQueueDisc = 0;
  for (size_t i = 1; i <= stats.size(); i++)
  // stats indexing needs to start from 1
  {
    if (stats[i].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE_DISC)
    {
      packetsDroppedByQueueDisc = packetsDroppedByQueueDisc + stats[i].packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
      bytesDroppedByQueueDisc = bytesDroppedByQueueDisc + stats[i].bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
    }
  }
  std::cout << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc
              << " / " << bytesDroppedByQueueDisc << std::endl;
  
  uint32_t packetsDroppedByNetDevice = 0;
  uint64_t bytesDroppedByNetDevice = 0;
  for (size_t i = 1; i <= stats.size(); i++)
  // stats indexing needs to start from 1
  {
    if (stats[i].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE)
    {
      packetsDroppedByNetDevice = packetsDroppedByNetDevice + stats[i].packetsDropped[Ipv4FlowProbe::DROP_QUEUE];
      bytesDroppedByNetDevice = bytesDroppedByNetDevice + stats[i].bytesDropped[Ipv4FlowProbe::DROP_QUEUE];
    }
  }
  std::cout << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice
              << " / " << bytesDroppedByNetDevice << std::endl;
  
  double TpT = 0;
  for (size_t i = 1; i <= stats.size(); i++)
  // stats indexing needs to start from 1
  // sum all the flows [bybes], multiply by 8 to get [bits] and divide by the time duration of each flow [seconds]. devide the final result by 1,000,000 to get [Mbps]                               
  {
    TpT = TpT + (stats[i].rxBytes * 8.0 / (stats[i].timeLastRxPacket.GetSeconds () - stats[i].timeFirstRxPacket.GetSeconds ())) / 1000000;
  }
  std::cout << "  Throughput: " << TpT << " Mbps" << std::endl;
                                  
  double AVGDelaySum = 0;
  double AVGDelay = 0;
  for (size_t i = 1; i <= stats.size(); i++)
  {
    AVGDelaySum = AVGDelaySum + stats[i].delaySum.GetSeconds () / stats[i].rxPackets;
  }
  AVGDelay = AVGDelaySum / stats.size();
  std::cout << "  Mean delay:   " << AVGDelay << std::endl;
  
  double AVGJitterSum = 0;
  double AVGJitter = 0;
  for (size_t i = 1; i <= stats.size(); i++)
  {
    AVGJitterSum = AVGJitterSum + stats[i].jitterSum.GetSeconds () / (stats[i].rxPackets - 1);
  }
  AVGJitter = AVGJitterSum / stats.size();
  std::cout << "  Mean jitter:   " << AVGJitter << std::endl;

  // Simulator::Destroy ();
  
  std::cout << std::endl << "*** Application statistics ***" << std::endl;
  std::cout << "  Total OnOff Tx Packets: " << g_totalOnOffTxPackets << std::endl;
  std::cout << "  Total OnOff Tx Bytes: " << g_totalOnOffTxBytes << std::endl;
  // A time-series file of cumulative OnOff Tx is available at: OnOff_TxPackets.dat
  
  uint64_t totalBytesRx = 0;
  for (size_t i = 0; i < sinkApps.GetN(); i++)
  {
    totalBytesRx = totalBytesRx + DynamicCast<PacketSink> (sinkApps.Get (i))->GetTotalRx ();
  }
  std::cout << "  Rx Bytes: " << totalBytesRx << std::endl;
  // double goodTpT = 0;
  // goodTpT = totalBytesRx * 8 / (END_TIME * 1000000.0); //Mbit/s
  // std::cout << "  Average Goodput: " << goodTpT << " Mbit/s" << std::endl;

  std::cout << std::endl << "*** TC Layer statistics ***" << std::endl;
  std::cout << tc->GetStats() << std::endl;

  std::cout << std::endl << "*** QueueDisc statistics ***" << std::endl;
  for (size_t i = 0; i < qdiscs.GetN(); i++)
  {
    std::cout << "Queue Disceplene " << i << ":" << std::endl;
    std::cout << qdiscs.Get(i)->GetStats () << std::endl;
  }

  
  // Added to create a .txt file with the summary of the tested scenario statistics
  std::ofstream testFlowStatistics (dirToSave + "/Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: 2In2Out" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + traffic_control_type << std::endl;
  testFlowStatistics << "Implementation Method: " + implementation << std::endl;
  testFlowStatistics << "Used D value: " + DoubleToString(miceElephantProb) << std::endl;
  testFlowStatistics << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
  testFlowStatistics << "Application: " + applicationType << std::endl; 
  testFlowStatistics << "Transport Protocol: " + transportProt << std::endl;
  testFlowStatistics << "traffic Mode " + onOffTrafficMode + "Random" << std::endl;
  testFlowStatistics << "Traffic Duration: " + DoubleToString(trafficGenDuration) + " [Sec]" << std::endl;
  testFlowStatistics << std::endl << "*** Flow monitor statistics ***" << std::endl;
  testFlowStatistics << "  Tx Packets/Bytes:   " << statTxPackets << " / " << statTxBytes << std::endl;
  testFlowStatistics << "  Rx Packets/Bytes:   " << statRxPackets << " / " << statRxBytes << std::endl;
  testFlowStatistics << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc
                      << " / " << bytesDroppedByQueueDisc << std::endl;
  testFlowStatistics << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice
                      << " / " << bytesDroppedByNetDevice << std::endl;
  testFlowStatistics << "  Throughput: " << TpT << " Mbps" << std::endl;
  testFlowStatistics << std::endl << "*** Application statistics ***" << std::endl;
  testFlowStatistics << "  Total OnOff Tx Packets: " << g_totalOnOffTxPackets << std::endl;
  testFlowStatistics << "  Total OnOff Tx Bytes: " << g_totalOnOffTxBytes << std::endl;
  testFlowStatistics << "  Rx Bytes: " << totalBytesRx << std::endl;                  
  testFlowStatistics << std::endl << "*** TC Layer statistics ***" << std::endl;
  testFlowStatistics << tc->GetStats () << std::endl;
  testFlowStatistics << std::endl << "*** QueueDisc Layer statistics ***" << std::endl;
  for (size_t i = 0; i < qdiscs.GetN(); i++)
  {
    testFlowStatistics << "Queue Disceplene " << i << ":" << std::endl;
    testFlowStatistics << qdiscs.Get(i)->GetStats () << std::endl;
  }

  testFlowStatistics.close ();

  // move all the produced files to dirToSave
  system (("mv -f " + dir + traffic_control_type + "/" + implementation + "/*.dat " + dirToSave).c_str ());
  // OnOff_TxPackets.dat is written directly into runDir by the trace callback. No extra move required.
  // system (("mv -f " + dir + traffic_control_type + "/" + implementation + "/*.txt " + dirToSave).c_str ());

  // Create the gnuplot.//////////////////////////////
  generate1DGnuPlotFromDatFile(dirToSave + "/generatedOnOffTrafficTrace.dat");
  generate1DGnuPlotFromDatFile(dirToSave + "/port_0_app_0_OnOffStateTrace.dat");
  generate1DGnuPlotFromDatFile(dirToSave + "/port_0_app_1_OnOffStateTrace.dat");
  generate1DGnuPlotFromDatFile(dirToSave + "/port_1_app_3_OnOffStateTrace.dat");
  generate1DGnuPlotFromDatFile(dirToSave + "/TxCongestionWindowChange.dat");
  generate1DGnuPlotFromDatFile(dirToSave + "/TrafficControl_Drop.dat");

  generate2DGnuPlotFromDatFile(dirToSave + "/port_0_queue_0_PacketsInQueueTrace.dat", "High");
  generate2DGnuPlotFromDatFile(dirToSave + "/port_1_queue_3_PacketsInQueueTrace.dat", "Low");

  Simulator::Destroy ();
  NS_LOG_INFO ("Stop simulation");

  return 0;
}
