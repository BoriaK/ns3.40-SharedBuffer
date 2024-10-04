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
#include <string>
#include <list>
#include <array>
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


// There are 2 servers connecting to a leaf switch
#define SERVER_COUNT 2
#define SWITCH_COUNT 1
#define RECIEVER_COUNT 2

#define SWITCH_RECIEVER_CAPACITY  500000        // Leaf-Spine Capacity 500Kbps/queue/port
#define SERVER_SWITCH_CAPACITY 5000000          // Total Serever-Leaf Capacity 5Mbps/queue/port
#define LINK_LATENCY MicroSeconds(20)           // each link latency 10 MicroSeconds 
#define BUFFER_SIZE 500                         // Shared Buffer Size for a single queue per port. 500 Packets

// The simulation starting and ending time
#define START_TIME 0.0
#define END_TIME 30

// The flow port range, each flow will be assigned a random port number within this range
#define SERV_PORT_P0 50000
#define SERV_PORT_P1 50001
#define SERV_PORT_P2 50002
#define SERV_PORT_P3 50003
#define SERV_PORT_P4 50004
 

// Adopted from the simulation from snowzjx
// Acknowledged to https://github.com/snowzjx/ns3-load-balance.git
#define PACKET_SIZE 1024 // 1024 [bytes]

using namespace ns3;

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

// std::string dir = "./Trace_Plots/2In2Out/test_Alphas/" + ToString(alpha_high) + "_" + ToString(alpha_low) + "/";
std::string dir = "./Trace_Plots/2In2Out/";
std::string traffic_control_type = "SharedBuffer_DT"; // "SharedBuffer_DT"/"SharedBuffer_FB"
std::string implementation = "via_MultiQueues/5_ToS";  // "via_NetDevices"/"via_FIFO_QueueDiscs"/"via_MultiQueues"
std::string usedAlgorythm;  // "DT"/"FB"
std::string onOffTrafficMode = "Uniform"; // "Constant"/"Uniform"/"Normal"

// functions to monitor Shared Buffer packets on Traffic Control Layer
void
TrafficControlPacketsInSharedQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream tcpisq (dir + traffic_control_type + "/" + implementation + "/TrafficControlPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  tcpisq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tcpisq.close ();
  
  std::cout << "PacketsInSharedBuffer: " << newValue << std::endl;
}

void
TrafficControlHighPriorityPacketsInSharedQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream tchppisq (dir + traffic_control_type + "/" + implementation + "/TrafficControlHighPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  tchppisq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tchppisq.close ();
  
  std::cout << "HighPriorityPacketsInSharedBuffer: " << newValue << std::endl;
}

void
TrafficControlLowPriorityPacketsInSharedQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream tclppisq (dir + traffic_control_type + "/" + implementation + "/TrafficControlLowPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  tclppisq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tclppisq.close ();
  
  std::cout << "LowPriorityPacketsInSharedBuffer: " << newValue << std::endl;
}

// Trace the Threshold Value for High Priority packets in the Shared Queue
void
TrafficControlThresholdHighTrace (size_t index, float_t oldValue, float_t newValue)  // added by me, to monitor Threshold
{
  std::ofstream tchpthr (dir + traffic_control_type + "/" + implementation + "/TrafficControlHighPriorityQueueThreshold_" + ToString(index) + ".dat", std::ios::out | std::ios::app);
  tchpthr << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tchpthr.close ();

  std::cout << "HighPriorityQueueThreshold on port: " << index << " is: " << newValue << " packets " << std::endl;
}

// Trace the Threshold Value for Low Priority packets in the Shared Queue
void
TrafficControlThresholdLowTrace (size_t index, float_t oldValue, float_t newValue)  // added by me, to monitor Threshold
{
  std::ofstream tclpthr (dir + traffic_control_type + "/" + implementation + "/TrafficControlLowPriorityQueueThreshold_" + ToString(index) + ".dat", std::ios::out | std::ios::app);
  tclpthr << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tclpthr.close ();
  
  std::cout << "LowPriorityQueueThreshold on port: " << index << " is: " << newValue << " packets " << std::endl;
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
  
  std::cout << "QueueDiscPacketsInPort " << portIndex << " Queue " << queueIndex << ": " << newValue << std::endl;
}

void
HighPriorityQueueDiscPacketsInQueueTrace (size_t portIndex, size_t queueIndex, uint32_t oldValue, uint32_t newValue)
{
  std::ofstream hpqdpiq (dir + traffic_control_type + "/" + implementation + "/port_" + ToString(portIndex) + "_queue_" + ToString(queueIndex) + "_HighPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  hpqdpiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  hpqdpiq.close ();
  
  std::cout << "HighPriorityQueueDiscPacketsInPort " << portIndex << " Queue " << queueIndex << ": " << newValue << std::endl;
}

void
LowPriorityQueueDiscPacketsInQueueTrace (size_t portIndex, size_t queueIndex, uint32_t oldValue, uint32_t newValue)
{
  std::ofstream lpqdpiq (dir + traffic_control_type + "/" + implementation + "/port_" + ToString(portIndex) + "_queue_" + ToString(queueIndex) + "_LowPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  lpqdpiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  lpqdpiq.close ();
  
  std::cout << "LowPriorityQueueDiscPacketsInPort " << portIndex << " Queue " << queueIndex << ": " << newValue << std::endl;
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
SojournTimeTrace (Time sojournTime)
{
  std::cout << "Sojourn time " << sojournTime.ToDouble (Time::MS) << "ms" << std::endl;
}

void
viaMQueuesPredictive5ToS_v2 (std::string traffic_control_type, std::string onoff_traffic_mode, double_t mice_elephant_prob, bool accumulate_stats)
{
  LogComponentEnable ("2In2Out", LOG_LEVEL_INFO);

  std::string implementation = "via_MultiQueues/5_ToS";
  std::string usedAlgorythm;
  std::string applicationType = "prioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"prioOnOff"
  std::string transportProt = "UDP"; // "UDP"/"TCP"
  std::string socketType;
  std::string queue_capacity;
  bool eraseOldData = true; // true/false
  
  // for local d estimatiuon
  // Tau = 50 [msec]. start predictive model at t0 - Tau/2. use window of: [t0 - Tau/2, t0 + Tau/2]
  double_t tau = 0.05; // [Sec]

  std::string  dir = datDir;
  if (traffic_control_type.compare("SharedBuffer_DT") == 0)
  {
    usedAlgorythm = "PredictiveDT";
  }


  if (traffic_control_type.compare("SharedBuffer_DT") == 0)
  {
    usedAlgorythm = "PredictiveDT";
  }
  else if (traffic_control_type.compare("SharedBuffer_FB") == 0)
  {
    usedAlgorythm = "PredictiveFB";
  }

  NS_LOG_INFO ("Config parameters");
  // Application type dependent parameters
  if (applicationType.compare("standardClient") == 0 || applicationType.compare("prioClient") == 0)
  {
    queue_capacity = "20p"; // B, the total space on the buffer.
  }
  else
  {
    queue_capacity = ToString(5 * BUFFER_SIZE) + "p"; // B, the total space on the buffer [packets]
    // queue_capacity = ToString(7 * BUFFER_SIZE) + "p"; // B, the total space on the buffer [packets]
  }

  // client type dependant parameters:
  if (transportProt.compare ("TCP") == 0)
  {
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
  uint64_t serverSwitchCapacity = 5 * SERVER_SWITCH_CAPACITY;
  n2s.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (serverSwitchCapacity)));
  n2s.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
  n2s.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));  // set basic queues to 1 packet
  // setting routers
  uint64_t switchRecieverCapacity = 5 * SWITCH_RECIEVER_CAPACITY;
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
  for (int i = 0; i < SERVER_COUNT; i++)
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


  for (int i = 0; i < RECIEVER_COUNT; i++)
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
  
  // Priomap prioMap = Priomap{prioArray};
  TosMap tosMap = TosMap{prioArray};
  // uint16_t handle = tch.SetRootQueueDisc("ns3::RoundRobinPrioQueueDisc", "Priomap", PriomapValue(prioMap));
  uint16_t handle = tch.SetRootQueueDisc("ns3::RoundRobinTosQueueDisc", "TosMap", TosMapValue(tosMap));

  TrafficControlHelper::ClassIdList cid = tch.AddQueueDiscClasses(handle, 5, "ns3::QueueDiscClass");
  tch.AddChildQueueDisc(handle, cid[0], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[0] is band "0" - the Highest Priority band
  tch.AddChildQueueDisc(handle, cid[1], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[1] is < cid[0]
  tch.AddChildQueueDisc(handle, cid[2], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[2] is < cid[1]
  tch.AddChildQueueDisc(handle, cid[3], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[3] is < cid[2]
  tch.AddChildQueueDisc(handle, cid[4], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[4] is < cid[3]

  QueueDiscContainer qdiscs = tch.Install (switchDevicesOut);  // in this option we installed TCH on switchDevicesOut. to send data from switch to reciever

  ///////// set the Traffic Controll layer to be a shared buffer////////////////////////
  TcPriomap tcPrioMap = TcPriomap{prioArray};
  Ptr<TrafficControlLayer> tc;
  tc = router.Get(0)->GetObject<TrafficControlLayer>();
  tc->SetAttribute("SharedBuffer", BooleanValue(true));
  tc->SetAttribute("MaxSharedBufferSize", StringValue (queue_capacity));
  tc->SetAttribute("TrafficControllAlgorythm", StringValue (usedAlgorythm));
  tc->SetAttribute("PriorityMapforMultiQueue", TcPriomapValue(tcPrioMap));
  tc->SetAttribute("TrafficEstimationWindowLength", DoubleValue(tau));

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
  tcPredict->SetAttribute("MaxSharedBufferSize", StringValue ("1p")); // no packets are actualy being stored in tcPredict
  tcPredict->SetAttribute("PriorityMapforMultiQueue", TcPriomapValue(tcPrioMap));

  //////////////Monitor data from q-disc//////////////////////////////////////////
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
  // for predictive model
  Ipv4InterfaceContainer serverIFsPredict;
  Ipv4InterfaceContainer switchInIFsPredict;
  Ipv4InterfaceContainer switchOutIFsPredict;
  Ipv4InterfaceContainer recieverIFsPredict;



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

  for (int i = 0; i < RECIEVER_COUNT; i++)
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

  std::vector<uint32_t> serverPort_vector;
  for (size_t i = 0; i < 5; i++)
  {
    serverPort_vector.push_back(50000 + i);
  }

  // create a vector of IP_ToS_Priorities: P0>P1>...>P4
  std::vector<uint32_t> ipTos_vector;
  uint32_t ipTos_P0 = 0x08; 
  ipTos_vector.push_back(ipTos_P0);
  uint32_t ipTos_P1 = 0x06; 
  ipTos_vector.push_back(ipTos_P1);
  uint32_t ipTos_P2 = 0x04; 
  ipTos_vector.push_back(ipTos_P2);
  uint32_t ipTos_P3 = 0x02; 
  ipTos_vector.push_back(ipTos_P3);
  uint32_t ipTos_P4 = 0x00; 
  ipTos_vector.push_back(ipTos_P4);
  
  
  ApplicationContainer sinkApps, sourceApps, sourceAppsPredict;

  // time interval values for OnOff Aplications
  double_t miceOffTime = 0.01; // [sec]
  double_t elephantOffTime = 0.1; // [sec]
  // for RNG:
  double_t miceOffTimeMax = 2 * miceOffTime; // [sec]
  double_t elephantOffTimeMax = 2 * elephantOffTime; // [sec]

  double_t Num_M_High = mice_elephant_prob * 10; // total number of OnOff machines that generate High priority traffic
  // int Num_E = 10 - Num_M_High; // number of OnOff machines that generate Low priority traffic
  bool unequalNum = false; // a flag that's raised if the number of High/Low priority OnOff applications created is diffeent for each Port
  if ((int(mice_elephant_prob * 10) % 2) != 0)
  {
    unequalNum = true;
  }
   
  for (size_t i = 0; i < 2; i++)
  {
    int serverIndex = i;
    int recieverIndex = i;
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
    
    // create InetSocketAddresses and PrioOnOffHelper vectors
    std::vector<InetSocketAddress> socketAddress_vector;
    std::vector<InetSocketAddress> socketAddressPredict_vector;
    std::vector<PrioOnOffHelper> clientHelpers_vector;
    std::vector<PrioOnOffHelper> clientHelpersPredict_vector;
    for (size_t j = 0; j < 5; j++)
    {
      InetSocketAddress tempSocketAddress = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), serverPort_vector[j]);
      tempSocketAddress.SetTos(ipTos_vector[j]); 
      socketAddress_vector.push_back(tempSocketAddress);
      // for preddictive model
      InetSocketAddress tempSocketAddressPredict = InetSocketAddress (recieverIFsPredict.GetAddress(recieverIndex), serverPort_vector[j]);
      tempSocketAddressPredict.SetTos(ipTos_vector[j]); 
      socketAddressPredict_vector.push_back(tempSocketAddressPredict);

      PrioOnOffHelper tempClientHelper(socketType, socketAddress_vector[j]);
      clientHelpers_vector.push_back(tempClientHelper);
      // for preddictive model
      PrioOnOffHelper tempClientHelperPredict(socketType, socketAddressPredict_vector[j]);
      clientHelpersPredict_vector.push_back(tempClientHelperPredict);
    }

    // create and install Client apps:    
    if (applicationType.compare("prioOnOff") == 0) 
    {
      for (size_t j = 0; j < 5; j++) 
      {
        clientHelpers_vector[j].SetAttribute ("Remote", AddressValue (socketAddress_vector[j]));
        // for predicting traffic in queue
        clientHelpersPredict_vector[j].SetAttribute ("Remote", AddressValue (socketAddressPredict_vector[j]));

        if (unequalNum) 
        {
          // in case the number of High/Low priority OnOff applications is not equally devidable by 2, 
          // divide the High/Low priority OnOff applications between the 2 servers such that:
          // 1st server recives the higher number of High priority and lower number of Low priority OnOff applications
          // and the 2nd server recives lower number of High priority and higher number of Low priority OnOff applications
          if (i == 0 && j < int(ceil(Num_M_High/2)) || i == 1 && j < int(floor(Num_M_High/2)))
          {
            if (onoff_traffic_mode.compare("Constant") == 0)
            { 
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));

              // for Predictive model
              clientHelpersPredict_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
              clientHelpersPredict_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
            }
            else if (onoff_traffic_mode.compare("Uniform") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));

              // for Predictive model
              clientHelpersPredict_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
              clientHelpersPredict_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
            }
            else if (onoff_traffic_mode.compare("Normal") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));

              // for Predictive model
              clientHelpersPredict_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
              clientHelpersPredict_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
            }
            else 
            {
              std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
              exit(1);
            }
            clientHelpers_vector[j].SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
            // for Predictive model
            clientHelpersPredict_vector[j].SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
          }
          else
          {
            if (onoff_traffic_mode.compare("Constant") == 0)
            { 
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));

              // for Predictive model
              clientHelpersPredict_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpersPredict_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(elephantOffTime) +"]"));
            }
            else if (onoff_traffic_mode.compare("Uniform") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(elephantOnTimeMax) +"]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));

              // for Predictive model
              clientHelpersPredict_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(elephantOnTimeMax) +"]"));
              clientHelpersPredict_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
            }
            else if (onoff_traffic_mode.compare("Normal") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(elephantOffTime) +"]"));

              // for Predictive model
              clientHelpersPredict_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpersPredict_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(elephantOffTime) +"]"));
            }
            else 
            {
              std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
              exit(1);
            }
            clientHelpers_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low 
            // for Predictive model
            clientHelpersPredict_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
          }
        }
        else
        {
          if (j < int(Num_M_High/2)) // create OnOff machines that generate High priority traffic
          {
            if (onoff_traffic_mode.compare("Constant") == 0)
            { 
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));

              // for Predictive model
              clientHelpersPredict_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
              clientHelpersPredict_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
            }
            else if (onoff_traffic_mode.compare("Uniform") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));

              // for Predictive model
              clientHelpersPredict_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
              clientHelpersPredict_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
            }
            else if (onoff_traffic_mode.compare("Normal") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));

              // for Predictive model
              clientHelpersPredict_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
              clientHelpersPredict_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
            }
            else 
            {
              std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
              exit(1);
            }
            clientHelpers_vector[j].SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
            // for Predictive model
            clientHelpersPredict_vector[j].SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
          }
          else // create OnOff machines that generate Low priority traffic
          {
            if (onoff_traffic_mode.compare("Constant") == 0)
            { 
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));

              // for Predictive model
              clientHelpersPredict_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpersPredict_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(elephantOffTime) +"]"));
            }
            else if (onoff_traffic_mode.compare("Uniform") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(elephantOnTimeMax) +"]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));

              // for Predictive model
              clientHelpersPredict_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(elephantOnTimeMax) +"]"));
              clientHelpersPredict_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
            }
            else if (onoff_traffic_mode.compare("Normal") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(elephantOffTime) +"]"));

              // for Predictive model
              clientHelpersPredict_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpersPredict_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(elephantOffTime) +"]"));
            }
            else 
            {
              std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
              exit(1);
            }
            clientHelpers_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low 
            // for Predictive model
            clientHelpersPredict_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low 
          }
        }
        clientHelpers_vector[j].SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelpers_vector[j].SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        // clientHelpers_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelpers_vector[j].SetAttribute("MiceElephantProbability", StringValue (DoubleToString(mice_elephant_prob)));
        sourceApps.Add(clientHelpers_vector[j].Install (servers.Get(serverIndex)));
        clientHelpers_vector[j].AssignStreams(servers.Get(serverIndex), 0);

        // for Predictive model
        clientHelpersPredict_vector[j].SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelpersPredict_vector[j].SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        // clientHelpersPredict_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelpersPredict_vector[j].SetAttribute("MiceElephantProbability", StringValue (DoubleToString(mice_elephant_prob)));
        sourceAppsPredict.Add(clientHelpersPredict_vector[j].Install (serversPredict.Get(serverIndex)));
        clientHelpersPredict_vector[j].AssignStreams(serversPredict.Get(serverIndex), 0);
      }
    }
    else 
    {
      std::cerr << "unknown app type: " << applicationType << std::endl;
      exit(1);
    }
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

  // double_t trafficGenDuration = 2; // for a single OnOff segment
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds(1.0 + trafficGenDuration));

  sourceAppsPredict.Start (Seconds (1.0 - tau/2));
  sourceAppsPredict.Stop (Seconds(1.0 + trafficGenDuration - tau/2));

  sinkApps.Start (Seconds (START_TIME));
  sinkApps.Stop (Seconds (END_TIME + 0.1));
  
  NS_LOG_INFO ("Enabling flow monitor");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  // datDir = "./Trace_Plots/test_Alphas/"
  // dir = "datDir"
  std::string dirToSave =  dir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/";
  if (!((std::filesystem::exists(dirToSave)) && (std::filesystem::is_directory(dirToSave))))
  {
    system (("mkdir -p " + dirToSave).c_str ());
  }
  
  if (eraseOldData == true)
  {
    system (("rm " + dirToSave + "*.dat").c_str ()); // to erase the old .dat files and collect new data
    system (("rm " + dirToSave + "*.txt").c_str ()); // to erase the previous test run summary, and collect new data
    std::cout << std::endl << "***Erased Previous Data***\n" << std::endl;
  }

  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (END_TIME + 10));
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl << "Topology: 2In2Out" << std::endl;
  std::cout << std::endl << "Queueing Algorithm: " + traffic_control_type + "Predictive" << std::endl;
  std::cout << std::endl << "Implementation Method: " + implementation << std::endl;
  std::cout << std::endl << "Used D value: " + DoubleToString(mice_elephant_prob) << std::endl;
  std::cout << std::endl << "Traffic Duration: " + DoubleToString(trafficGenDuration) + " [Sec]" << std::endl;
  std::cout << std::endl << "Application: " + applicationType << std::endl;
  std::cout << std::endl << "traffic Mode " + onoff_traffic_mode + "Random" << std::endl;

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
  // a loop to sum the Tx/Rx Packets/Bytes from all nodes
  uint32_t statTxPackets = 0; 
  uint64_t statTxBytes = 0;
  uint32_t statRxPackets = 0; 
  uint64_t statRxBytes = 0;
  for (size_t i = 1; i <= flowStats.size(); i++) // stats indexing needs to start from 1
  {
    if (flowStats[i].rxPackets > 0) //count only packets sent by the actual model (not the predictive packets)
    {
      statTxPackets = statTxPackets + flowStats[i].txPackets;
      statTxBytes = statTxBytes + flowStats[i].txBytes;
      statRxPackets = statRxPackets + flowStats[i].rxPackets;
      statRxBytes = statRxBytes + flowStats[i].rxBytes;
    }
  }

  std::cout << "  Tx Packets/Bytes:   " << statTxPackets
              << " / " << statTxBytes << std::endl;
  std::cout << "  Rx Packets/Bytes:   " << statRxPackets
              << " / " << statRxBytes << std::endl;

  uint32_t packetsDroppedByQueueDisc = 0;
  uint64_t bytesDroppedByQueueDisc = 0;
  for (size_t i = 1; i <= flowStats.size(); i++) // stats indexing needs to start from 1
  {
    if (flowStats[i].rxPackets > 0) //count only packets sent by the actual model (not the predictive packets)
    {
      if (flowStats[i].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE_DISC)
      {
        packetsDroppedByQueueDisc = packetsDroppedByQueueDisc + flowStats[i].packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
        bytesDroppedByQueueDisc = bytesDroppedByQueueDisc + flowStats[i].bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
      }
    }
  }
  std::cout << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc
              << " / " << bytesDroppedByQueueDisc << std::endl;
  
  uint32_t packetsDroppedByNetDevice = 0;
  uint64_t bytesDroppedByNetDevice = 0;
  for (size_t i = 1; i <= flowStats.size(); i++) // stats indexing needs to start from 1
  {
    if (flowStats[i].rxPackets > 0) //count only packets sent by the actual model (not the predictive packets)
    {
      if (flowStats[i].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE)
      {
        packetsDroppedByNetDevice = packetsDroppedByNetDevice + flowStats[i].packetsDropped[Ipv4FlowProbe::DROP_QUEUE];
        bytesDroppedByNetDevice = bytesDroppedByNetDevice + flowStats[i].bytesDropped[Ipv4FlowProbe::DROP_QUEUE];
      }
    }
  }
  std::cout << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice
              << " / " << bytesDroppedByNetDevice << std::endl;
  
  double TpT = 0;
  for (size_t i = 1; i <= flowStats.size(); i++) // stats indexing needs to start from 1
  {
    if (flowStats[i].rxPackets > 0) //count only packets sent by the actual model (not the predictive packets)
    {
      TpT = TpT + (flowStats[i].rxBytes * 8.0 / (flowStats[i].timeLastRxPacket.GetSeconds () - flowStats[i].timeFirstRxPacket.GetSeconds ())) / 1000000;
    }
  }
  std::cout << "  Throughput: " << TpT << " Mbps" << std::endl;
                                  
  double AVGDelaySum = 0;
  double AVGDelay = 0;
  for (size_t i = 1; i <= flowStats.size(); i++) // stats indexing needs to start from 1
  {
    if (flowStats[i].rxPackets > 0) //count only packets sent by the actual model (not the predictive packets)
    {
    AVGDelaySum = AVGDelaySum + flowStats[i].delaySum.GetSeconds () / flowStats[i].rxPackets;
    }
  }
  AVGDelay = AVGDelaySum / (flowStats.size()/2);
  std::cout << "  Mean delay:   " << AVGDelay << std::endl;
  
  double AVGJitterSum = 0;
  double AVGJitter = 0;
  for (size_t i = 1; i <= flowStats.size(); i++) // stats indexing needs to start from 1
  {
    if (flowStats[i].rxPackets > 0) //count only packets sent by the actual model (not the predictive packets)
    {
    AVGJitterSum = AVGJitterSum + flowStats[i].jitterSum.GetSeconds () / (flowStats[i].rxPackets - 1);
    }
  }
  AVGJitter = AVGJitterSum / (flowStats.size()/2);
  std::cout << "  Mean jitter:   " << AVGJitter << std::endl;

  // Simulator::Destroy ();
  
  std::cout << std::endl << "*** Application statistics ***" << std::endl;
  double goodTpT = 0;

  uint64_t totalBytesRx = 0;
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
  std::ofstream testFlowStatistics (dirToSave + "Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: 2In2Out" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + traffic_control_type + "Predictive" << std::endl;
  testFlowStatistics << "Implementation Method: " + implementation << std::endl;
  testFlowStatistics << "Used D value: " + DoubleToString(mice_elephant_prob) << std::endl;
  testFlowStatistics << "Traffic Duration: " + DoubleToString(trafficGenDuration) + " [Sec]" << std::endl;
  testFlowStatistics << "Application: " + applicationType << std::endl;
  testFlowStatistics << "traffic Mode " + onoff_traffic_mode + "Random" << std::endl; 
  testFlowStatistics << std::endl << "*** Flow monitor statistics ***" << std::endl;
  testFlowStatistics << "  Tx Packets/Bytes:   " << statTxPackets << " / " << statTxBytes << std::endl;
  testFlowStatistics << "  Rx Packets/Bytes:   " << statRxPackets << " / " << statRxBytes << std::endl;
  testFlowStatistics << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc
                      << " / " << bytesDroppedByQueueDisc << std::endl;
  testFlowStatistics << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice
                      << " / " << bytesDroppedByNetDevice << std::endl;
  testFlowStatistics << "  Throughput: " << TpT << " Mbps" << std::endl;                   
  testFlowStatistics << std::endl << "*** TC Layer statistics ***" << std::endl;
  testFlowStatistics << tcStats << std::endl;
  testFlowStatistics << std::endl << "*** QueueDisc Layer statistics ***" << std::endl;
  for (size_t i = 0; i < qdiscs.GetN(); i++)
  {
    testFlowStatistics << "Queue Disceplene " << i << ":" << std::endl;
    testFlowStatistics << qdiscs.Get(i)->GetStats () << std::endl;
  }
  
  testFlowStatistics.close ();

  // move all the produced .dat files to a directory based on the Alpha values
  // datDir = "./Trace_Plots/test_Alphas/"
  // dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/";
  std::string newDir = dirToSave + DoubleToString(mice_elephant_prob) + "/Predictive/";
  // std::string newDir = dirToSave;
  system (("mkdir -p " + newDir).c_str ());
  system (("mv -f " + datDir + "*.dat " + newDir).c_str ());
  system (("mv -f " + dirToSave + "*.txt " + newDir).c_str ());

  // if chose to acumulate statistics:
  if (accumulate_stats)
  {
    if (!(std::filesystem::exists( dir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                  + usedAlgorythm + "_TestAccumulativeStatistics.dat")))
    {
      // If the file doesn't exist, create it and write initial statistics
      system (("mkdir -p " +  dir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/").c_str ());
      std::ofstream testAccumulativeStats ( dir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                            + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      testAccumulativeStats
      << tcStats.nTotalDroppedPackets << " " 
      << tcStats.nTotalDroppedPacketsHighPriority << " " 
      << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
      testAccumulativeStats.close ();
    }
    else
    {
      // Open the file in append mode
      std::fstream testAccumulativeStats ( dir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                          + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      testAccumulativeStats
      << tcStats.nTotalDroppedPackets << " " 
      << tcStats.nTotalDroppedPacketsHighPriority << " " 
      << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
      testAccumulativeStats.close ();
    }
  }
  // clear all statistics for this simulation itteration
  flowStats.clear();
  // delete tcStats;  doesn't work!
  tcStats.~TCStats();

  // Simulator::Cancel();
  Simulator::Destroy ();
  NS_LOG_INFO ("Stop simulation");
}
