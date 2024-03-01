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
 * Basic Topology with 2 clients: n0 and n1, a switch S, and 2 recievers r0 and r1
 * this design is based on DumbellTopoplogy model.
 * in this version all the NetDevices are created first and the TrafficControllHelper is installed on them simultanoiusly
 *
 *     t0__      _____      __r0
 *     t1__|--p0|     |p0--|__r1
 *     t2__     |  S  |     __r2
 *     t3__|--p1|_____|p1--|__r3
 * 
 * 
 * Additional "model" at t-T to predict traffic:
 *     t0__      _____      __r0
 *     t1__|--p0|     |p0--|__r1
 *     t2__     |  S  |     __r2
 *     t3__|--p1|_____|p1--|__r3
 * 
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <string>
#include <list>
#include <array>

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

// Adopted from the simulation from snowzjx
// Acknowledged to https://github.com/snowzjx/ns3-load-balance.git
#define PACKET_SIZE 1024 // 1024 [bytes]

using namespace ns3;

std::string dir = "./Trace_Plots/2In2Out/";
std::string traffic_control_type = "SharedBuffer_PredictiveFB_v01"; // "SharedBuffer_DT_v01"/"SharedBuffer_FB_v01"/"SharedBuffer_PredictiveFB_v01"
std::string implementation = "via_MultiQueues/2_ToS";  // "via_NetDevices"/"via_FIFO_QueueDiscs"/"via_MultiQueues"
std::string usedAlgorythm;  // "DT"/"FB"/"PredictiveFB"

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
IntToString (u_int32_t value)
{
  std::stringstream ss;
  ss << value;
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

// for debug:
void
TrafficControlPredictPacketsInSharedQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream tcpisq (dir + traffic_control_type + "/" + implementation + "/TrafficControlPredictPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  tcpisq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tcpisq.close ();
  
  // std::cout << "PacketsInSharedBuffer: " << newValue << std::endl;
}

void
TrafficControlPredictHighPriorityPacketsInSharedQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream tchppisq (dir + traffic_control_type + "/" + implementation + "/TrafficControlPredictHighPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  tchppisq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  tchppisq.close ();
  
  // std::cout << "HighPriorityPacketsInSharedBuffer: " << newValue << std::endl;
}

void
TrafficControlPredictLowPriorityPacketsInSharedQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream tclppisq (dir + traffic_control_type + "/" + implementation + "/TrafficControlPredictLowPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
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

void
SojournTimeTrace (Time sojournTime)
{
  std::cout << "Sojourn time " << sojournTime.ToDouble (Time::MS) << "ms" << std::endl;
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("2In2Out", LOG_LEVEL_INFO);

  std::string applicationType = "prioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"prioOnOff"
  std::string onoff_traffic_mode = "Uniform"; // "Constant"/"Uniform"/"Normal"
  // Command line parameters parsing
  std::string transportProt = "UDP"; // "UDP"/"TCP"
  std::string socketType;
  std::string queue_capacity;
  if (traffic_control_type.compare("SharedBuffer_DT_v01") == 0)
  {
      usedAlgorythm = "DT";
  }
  else if (traffic_control_type.compare("SharedBuffer_FB_v01") == 0)
  {
      usedAlgorythm = "FB";
  }
  else if (traffic_control_type.compare("SharedBuffer_PredictiveFB_v01") == 0)
  {
      usedAlgorythm = "PredictiveFB";
  }
  double_t alpha_high = 10;
  double_t alpha_low = 2;
  uint32_t flowletTimeout = 50;
  bool eraseOldData = true; // true/false

  if (eraseOldData == true)
  {
      system (("rm " + dir + traffic_control_type + "/" + implementation + "/*.dat").c_str ()); // to erase the old .dat files and collect new data
      system (("rm " + dir + traffic_control_type + "/" + implementation + "/*.txt").c_str ()); // to erase the previous test run summary, and collect new data
      std::cout << std::endl << "***Erased Previous Data***\n" << std::endl;
  }

  CommandLine cmd;
  cmd.AddValue ("transportProt", "Transport protocol to use: Udp, Tcp, DcTcp", transportProt);
  cmd.AddValue ("flowletTimeout", "flowlet timeout", flowletTimeout);
  cmd.Parse (argc, argv);

  NS_LOG_INFO ("Config parameters");
  // Application type dependent parameters
  if (applicationType.compare("standardClient") == 0 || applicationType.compare("prioClient") == 0)
  {
    queue_capacity = "20p"; // B, the total space on the buffer.
  }
  else
  {
    queue_capacity = ToString(2 * BUFFER_SIZE) + "p"; // B, the total space on the buffer [packets]
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
  Names::Clear(); // for loop use. make sure name "Router" is not stored in Names map
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

  // NS_LOG_INFO ("Install channels and assign addresses");

  PointToPointHelper n2s, s2r, n2sPredict, s2rPredict;
  NS_LOG_INFO ("Configuring channels for all the Nodes");
  
  // Setting servers
  uint64_t serverSwitchCapacity =  2 * SERVER_SWITCH_CAPACITY;
  n2s.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (serverSwitchCapacity)));
  n2s.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
  n2s.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));  // set basic queues to 1 packet
  // setting routers
  uint64_t switchRecieverCapacity = 2 * SWITCH_RECIEVER_CAPACITY;
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

  for (size_t i = 0; i < switchDevicesOut.GetN(); i++)
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
  std::array<uint16_t, 16> prioArray = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  
  // Priomap prioMap = Priomap{prioArray};
  TosMap tosMap = TosMap{prioArray};
  uint16_t handle = tch.SetRootQueueDisc("ns3::RoundRobinTosQueueDisc", "TosMap", TosMapValue(tosMap));

  TrafficControlHelper::ClassIdList cid = tch.AddQueueDiscClasses(handle, 2, "ns3::QueueDiscClass");
  tch.AddChildQueueDisc(handle, cid[0], "ns3::FifoQueueDisc" , "MaxSize", StringValue (queue_capacity)); // cid[0] is band "0" - the Highest Priority band
  tch.AddChildQueueDisc(handle, cid[1], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[1] is Low Priority

  QueueDiscContainer qdiscs = tch.Install (switchDevicesOut);  // in this option we installed TCH on switchDevicesOut. to send data from switch to reciever
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
  tc->SetAttribute("TrafficControllAlgorythm", StringValue (usedAlgorythm));
  tc->SetAttribute("PriorityMapforMultiQueue", TcPriomapValue(tcPrioMap));

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
  tcPredict->SetAttribute("Alpha_High", DoubleValue (alpha_high));
  tcPredict->SetAttribute("Alpha_Low", DoubleValue (alpha_low));
  // tcPredict->SetAttribute("TrafficControllAlgorythm", StringValue ("FB"));  // use FB for predictive model for now
  tcPredict->SetAttribute("PriorityMapforMultiQueue", TcPriomapValue(tcPrioMap));

  ///////////Monitor data from q-disc//////////////////////////////////////////
  for (size_t i = 0; i < RECIEVER_COUNT; i++)  // over all ports
  {
      for (size_t j = 0; j < qdiscs.Get (i)->GetNQueueDiscClasses(); j++)  // over all the queues per port
      {
        Ptr<QueueDisc> queue = qdiscs.Get (i)->GetQueueDiscClass(j)->GetQueueDisc();
        queue->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&QueueDiscPacketsInQueueTrace, i, j));
        queue->TraceConnectWithoutContext ("HighPriorityPacketsInQueue", MakeBoundCallback (&HighPriorityQueueDiscPacketsInQueueTrace, i, j)); 
        queue->TraceConnectWithoutContext ("LowPriorityPacketsInQueue", MakeBoundCallback (&LowPriorityQueueDiscPacketsInQueueTrace, i, j)); 
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

  uint32_t ipTos_HP = 0x10;  //High priority: Maximize Throughput
  uint32_t ipTos_LP = 0x00; //Low priority: Best Effort
  
  ApplicationContainer sinkApps, sourceApps, sourceAppsPredict;
  
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
    
    InetSocketAddress socketAddressP0 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P0);
    socketAddressP0.SetTos(ipTos_HP);  // ToS 0x10 -> High priority
    InetSocketAddress socketAddressP1 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P1);
    socketAddressP1.SetTos(ipTos_LP);  // ToS 0x0 -> Low priority

    // for preddictive model
    InetSocketAddress socketAddressP0Predict = InetSocketAddress (recieverIFsPredict.GetAddress(recieverIndex), SERV_PORT_P0);
    socketAddressP0Predict.SetTos(ipTos_HP);  // ToS 0x10 -> High priority
    InetSocketAddress socketAddressP1Predict = InetSocketAddress (recieverIFsPredict.GetAddress(recieverIndex), SERV_PORT_P1);
    socketAddressP1Predict.SetTos(ipTos_LP);  // ToS 0x0 -> Low priority
    
    // time interval values for OnOff Aplications
    std::string miceOnTime = "0.05"; // [sec]
    std::string miceOffTime = "0.01"; // [sec]
    std::string elephantOnTime = "0.5"; // [sec]
    std::string elephantOffTime = "0.1"; // [sec]
    // for RNG:
    std::string miceOnTimeMax = "0.1"; // [sec]
    std::string miceOffTimeMax = "0.02"; // [sec]
    std::string elephantOnTimeMax = "1"; // [sec]
    std::string elephantOffTimeMax = "0.2"; // [sec]
    
    // create and install Client apps:    
    if (applicationType.compare("standardClient") == 0) 
    {
      // Install UDP application on the sender 
      // send packet flows from servers with even indexes to spine 0, and from servers with odd indexes to spine 1.

      UdpClientHelper clientHelperP0 (socketAddressP0);
      clientHelperP0.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
      clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));

      UdpClientHelper clientHelperP1 (socketAddressP1);
      clientHelperP1.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
      clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      sourceApps.Add(clientHelperP1.Install (servers.Get(serverIndex)));
    } 
    else if (applicationType.compare("OnOff") == 0) 
    {
      // Create the OnOff applications to send TCP/UDP to the server
      OnOffHelper clientHelperP0 (socketType, socketAddressP0);
      clientHelperP0.SetAttribute ("Remote", AddressValue (socketAddressP0));
      clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + miceOnTime + "]"));
      clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + miceOffTime + "]"));
      clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP0.SetAttribute ("DataRate", StringValue ("2Mb/s"));
      sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));

      OnOffHelper clientHelperP1 (socketType, socketAddressP1);
      clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
      clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + elephantOnTime + "]"));
      clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + elephantOffTime + "]"));
      clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP1.SetAttribute ("DataRate", StringValue ("2Mb/s"));
      sourceApps.Add(clientHelperP1.Install (servers.Get(serverIndex)));
    } 
    else if (applicationType.compare("prioClient") == 0)
    {
      UdpPrioClientHelper clientHelperP0 (socketAddressP0);
      clientHelperP0.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
      clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      // clientHelperP0.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP0.SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));
      
      UdpPrioClientHelper clientHelperP1 (socketAddressP1);
      clientHelperP1.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
      clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      // clientHelperP1.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP1.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP1.Install (servers.Get(serverIndex)));
    }
    else if (applicationType.compare("prioOnOff") == 0) 
    {
      // Create the OnOff applications to send TCP/UDP to the server
      PrioOnOffHelper clientHelperP0 (socketType, socketAddressP0);
      clientHelperP0.SetAttribute ("Remote", AddressValue (socketAddressP0));
      // make On time shorter to make high priority flows shorter
      if (onoff_traffic_mode.compare("Constant") == 0)
      {
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + miceOnTime + "]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + miceOffTime + "]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + miceOnTimeMax + "]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + miceOffTimeMax + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + miceOnTime + "|Variance=" + miceOnTime + "|Bound=" + miceOnTime + "]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + miceOffTime + "|Variance=" + miceOffTime + "|Bound=" + miceOffTime + "]"));
      }
      else 
      {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
      }
      clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP0.SetAttribute ("DataRate", StringValue ("2Mb/s"));
      // clientHelperP0.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP0.SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));
      clientHelperP0.AssignStreams(servers, 1);

      PrioOnOffHelper clientHelperP1 (socketType, socketAddressP1);
      clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
      if (onoff_traffic_mode.compare("Constant") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + elephantOnTime + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + elephantOffTime + "]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + elephantOnTimeMax + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + elephantOffTimeMax + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + elephantOnTime + "|Variance=" + elephantOnTime + "|Bound=" + elephantOnTime + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + elephantOffTime + "|Variance=" + elephantOffTime + "|Bound=" + elephantOffTime + "]"));
      }
      else 
      {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
      }
      clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP1.SetAttribute ("DataRate", StringValue ("2Mb/s"));
      // clientHelperP1.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP1.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP1.Install (servers.Get(serverIndex)));
      clientHelperP1.AssignStreams(servers, 1);

      // for predicting traffic in queue
      PrioOnOffHelper clientHelperP0Predict (socketType, socketAddressP0Predict);
      clientHelperP0Predict.SetAttribute ("Remote", AddressValue (socketAddressP0Predict));
      if (onoff_traffic_mode.compare("Constant") == 0)
      {
        clientHelperP0Predict.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + miceOnTime + "]"));
        clientHelperP0Predict.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + miceOffTime + "]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP0Predict.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + elephantOnTimeMax + "]"));
        clientHelperP0Predict.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + elephantOffTimeMax + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP0Predict.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + elephantOnTime + "|Variance=" + elephantOnTime + "|Bound=" + elephantOnTime + "]"));
        clientHelperP0Predict.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + elephantOffTime + "|Variance=" + elephantOffTime + "|Bound=" + elephantOffTime + "]"));
      }
      else 
      {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
      }
      clientHelperP0Predict.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP0Predict.SetAttribute ("DataRate", StringValue ("2Mb/s"));
      // clientHelperP0.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP0Predict.SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceAppsPredict.Add(clientHelperP0Predict.Install (serversPredict.Get(serverIndex)));
      clientHelperP0Predict.AssignStreams(serversPredict, 1);

      PrioOnOffHelper clientHelperP1Predict (socketType, socketAddressP1Predict);
      clientHelperP1Predict.SetAttribute ("Remote", AddressValue (socketAddressP1Predict));
      if (onoff_traffic_mode.compare("Constant") == 0)
      {
        clientHelperP1Predict.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + elephantOnTime + "]"));
        clientHelperP1Predict.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + elephantOffTime + "]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP1Predict.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + elephantOnTimeMax + "]"));
        clientHelperP1Predict.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + elephantOffTimeMax + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP1Predict.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + elephantOnTime + "|Variance=" + elephantOnTime + "|Bound=" + elephantOnTime + "]"));
        clientHelperP1Predict.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + elephantOffTime + "|Variance=" + elephantOffTime + "|Bound=" + elephantOffTime + "]"));
      }
      else 
      {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
      }
      clientHelperP1Predict.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP1Predict.SetAttribute ("DataRate", StringValue ("2Mb/s"));
      // clientHelperP1Predict.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP1Predict.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceAppsPredict.Add(clientHelperP1Predict.Install (serversPredict.Get(serverIndex)));
      clientHelperP1Predict.AssignStreams(serversPredict, 1);
    }
    else 
    {
      std::cerr << "unknown app type: " << applicationType << std::endl;
      exit(1);
    }
    // setup sinks
    Address sinkLocalAddressP0 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P0));
    Address sinkLocalAddressP1 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P1));
    PacketSinkHelper sinkP0 (socketType, sinkLocalAddressP0); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    PacketSinkHelper sinkP1 (socketType, sinkLocalAddressP1); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    sinkApps.Add(sinkP0.Install (recievers.Get(recieverIndex)));
    sinkApps.Add(sinkP1.Install (recievers.Get(recieverIndex)));     
  }

  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds(3.0));
  // Tau = miceOnTime + 0.01 [sec]. start predictive model at t0 - Tau
  sourceAppsPredict.Start (Seconds (0.64));
  sourceAppsPredict.Stop (Seconds(2.64));

  sinkApps.Start (Seconds (START_TIME));
  sinkApps.Stop (Seconds (END_TIME + 0.1));
  
  NS_LOG_INFO ("Enabling flow monitor");
  FlowMonitorHelper flowmon;
  // Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  ns3::NodeContainer activeTrafficNodes;
  activeTrafficNodes.Add(servers, router, recievers);
  Ptr<FlowMonitor> monitor = flowmon.Install(activeTrafficNodes);

  // Create a new directory to store the output of the program
  std::string dirToSave = "mkdir -p " + dir + traffic_control_type + "/" + implementation;
  if (system (dirToSave.c_str ()) == -1)
  {
      exit (1);
  }  

  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (END_TIME + 10));
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl << "Topology: 2In2Out" << std::endl;
  std::cout << std::endl << "Queueing Algorithm: " + traffic_control_type << std::endl;
  std::cout << std::endl << "Implementation Method: " + implementation << std::endl;
  std::cout << std::endl << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
  std::cout << std::endl << "Application: " + applicationType << std::endl;

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
  // a loop to sum the Tx/Rx Packets/Bytes from all nodes
  uint32_t statTxPackets = 0; 
  uint64_t statTxBytes = 0;
  uint32_t statRxPackets = 0; 
  uint64_t statRxBytes = 0;
  for (size_t i = 1; i <= flowStats.size(); i++)
  // stats indexing needs to start from 1
  {
      statTxPackets = statTxPackets + flowStats[i].txPackets;
      statTxBytes = statTxBytes + flowStats[i].txBytes;
      statRxPackets = statRxPackets + flowStats[i].rxPackets;
      statRxBytes = statRxBytes + flowStats[i].rxBytes;
  }

  std::cout << "  Tx Packets/Bytes:   " << statTxPackets
              << " / " << statTxBytes << std::endl;
  std::cout << "  Rx Packets/Bytes:   " << statRxPackets
              << " / " << statRxBytes << std::endl;

  uint32_t packetsDroppedByQueueDisc = 0;
  uint64_t bytesDroppedByQueueDisc = 0;
  for (size_t i = 1; i <= flowStats.size(); i++)
  // stats indexing needs to start from 1
  {
      if (flowStats[i].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE_DISC)
      {
      packetsDroppedByQueueDisc = packetsDroppedByQueueDisc + flowStats[i].packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
      bytesDroppedByQueueDisc = bytesDroppedByQueueDisc + flowStats[i].bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
      }
  }
  std::cout << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc
              << " / " << bytesDroppedByQueueDisc << std::endl;
  
  uint32_t packetsDroppedByNetDevice = 0;
  uint64_t bytesDroppedByNetDevice = 0;
  for (size_t i = 1; i <= flowStats.size(); i++)
  // stats indexing needs to start from 1
  {
      if (flowStats[i].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE)
      {
          packetsDroppedByNetDevice = packetsDroppedByNetDevice + flowStats[i].packetsDropped[Ipv4FlowProbe::DROP_QUEUE];
          bytesDroppedByNetDevice = bytesDroppedByNetDevice + flowStats[i].bytesDropped[Ipv4FlowProbe::DROP_QUEUE];
      }
  }
  std::cout << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice
              << " / " << bytesDroppedByNetDevice << std::endl;
  
  double TpT = 0;
  for (size_t i = 1; i <= flowStats.size(); i++)
  // stats indexing needs to start from 1
  {
      TpT = TpT + (flowStats[i].rxBytes * 8.0 / (flowStats[i].timeLastRxPacket.GetSeconds () - flowStats[i].timeFirstRxPacket.GetSeconds ())) / 1000000;
  }
  std::cout << "  Throughput: " << TpT << " Mbps" << std::endl;
                                  
  double AVGDelaySum = 0;
  double AVGDelay = 0;
  for (size_t i = 1; i <= flowStats.size(); i++)
  {
      AVGDelaySum = AVGDelaySum + flowStats[i].delaySum.GetSeconds () / flowStats[i].rxPackets;
  }
  AVGDelay = AVGDelaySum / flowStats.size();
  std::cout << "  Mean delay:   " << AVGDelay << std::endl;
  
  double AVGJitterSum = 0;
  double AVGJitter = 0;
  for (size_t i = 1; i <= flowStats.size(); i++)
  {
      AVGJitterSum = AVGJitterSum + flowStats[i].jitterSum.GetSeconds () / (flowStats[i].rxPackets - 1);
  }
  AVGJitter = AVGJitterSum / flowStats.size();
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
  std::ofstream testFlowStatistics (dir + traffic_control_type + "/" + implementation + "/Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: 2In2Out" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + traffic_control_type << std::endl;
  testFlowStatistics << "Implementation Method: " + implementation << std::endl;
  testFlowStatistics << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
  testFlowStatistics << "Application: " + applicationType << std::endl; 
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

  // command line needs to be in ./scratch/ inorder for the script to produce gnuplot correctly///
  // system (("gnuplot " + dir + "gnuplotScriptTcHighPriorityPacketsInQueue").c_str ());

  // clear all statistics for this simulation itteration
  // flowStats.clear();
  // tcStats.~TCStats();

  Simulator::Destroy ();
  NS_LOG_INFO ("Stop simulation");
  
  return 0;
}

