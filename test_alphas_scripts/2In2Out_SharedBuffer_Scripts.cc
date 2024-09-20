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
#include <vector>
#include <utility>
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

// There are 2 servers connecting to a leaf switch
#define SERVER_COUNT 2
#define SWITCH_COUNT 1
#define RECIEVER_COUNT 2

#define SWITCH_RECIEVER_CAPACITY  500000        // Leaf-Spine Capacity 500Kbps/queue/port
#define SERVER_SWITCH_CAPACITY 5000000          // Total Serever-Leaf Capacity 5Mbps/queue/port
#define LINK_LATENCY MicroSeconds(20)            // each link latency 10 MicroSeconds 
#define BUFFER_SIZE 500                         // Shared Buffer Size for a single queue/port. 250 [Packets]

// The simulation starting and ending time
#define START_TIME 0.0
#define DURATION_TIME 6
#define END_TIME 60

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

std::string datDir = "./Trace_Plots/test_Alphas/";

// for OnOff Aplications
uint32_t dataRate = 2; // [Mbps] data generation rate for a single OnOff application
// time interval values 
double_t trafficGenDuration = 2; // [sec] initilize for a single OnOff segment
int32_t numOfTotalPackets = 795; // [packets] number of total generated packets for a single OnOff Pair (High and Low)
double_t miceOnTime = 0.05; // [sec] for ~12 packets/flow
double_t elephantOnTime = 0.5; // [sec] for ~125 packets/flow

double_t miceOffTimeConst = 0.01; // [sec]
double_t elephantOffTimeConst = 0.1; // [sec]
// for RNG:
double_t miceOnTimeMax = 0.1; // [sec]
double_t elephantOnTimeMax = 1.0; // [sec]

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
  std::ofstream qdpiq (datDir + "/queueDisc_" + ToString(index) + "_PacketsInQueueTrace.dat", std::ios::out | std::ios::app);
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
  std::ofstream lpqdpiq (datDir  + "/queueDisc_" + ToString(index) + "_LowPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  lpqdpiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  lpqdpiq.close ();
  
  std::cout << "LowPriorityQueueDiscPacketsInQueue " << index << ": " << newValue << std::endl;
}

void
SojournTimeTrace (Time sojournTime)
{
  std::cout << "Sojourn time " << sojournTime.ToDouble (Time::MS) << "ms" << std::endl;
}


void
viaFIFO(std::string traffic_control_type, std::string onoff_traffic_mode, double_t mice_elephant_prob, double_t alpha_high, double_t alpha_low, bool accumulate_stats)
{
  LogComponentEnable ("2In2Out", LOG_LEVEL_INFO);

  std::string implementation = "via_FIFO_QueueDisc";
  std::string usedAlgorythm;
  std::string applicationType = "prioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"prioOnOff"
  std::string transportProt = "UDP"; // "UDP"/"TCP"
  std::string socketType;
  std::string queue_capacity;
  if (traffic_control_type.compare("SharedBuffer_DT") == 0)
  {
      usedAlgorythm = "DT";
  }
  else if (traffic_control_type.compare("SharedBuffer_FB") == 0)
  {
      usedAlgorythm = "FB";
  }

  bool eraseOldData = true; // true/false

  if (eraseOldData == true)
  {
      system (("rm " + datDir + traffic_control_type + "/" + implementation + "/*.dat").c_str ()); // to erase the old .dat files and collect new data
      system (("rm " + datDir + traffic_control_type + "/" + implementation + "/*.txt").c_str ()); // to erase the previous test run summary, and collect new data
      std::cout << std::endl << "***Erased Previous Data***\n" << std::endl;
  }

  NS_LOG_INFO ("Config parameters");
  // Application type dependent parameters
  if (applicationType.compare("standardClient") == 0 || applicationType.compare("prioClient") == 0)
  {
    queue_capacity = "20p"; // B, the total space on the buffer.
  }
  else
  {
    queue_capacity = ToString(BUFFER_SIZE) + "p"; // B, the total space on the buffer [packets]
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
  Names::Add("Router", router.Get(0));  // Add a Name to the router node
  NodeContainer servers;
  servers.Create (SERVER_COUNT);

  // NS_LOG_INFO ("Install channels and assign addresses");

  PointToPointHelper n2s, s2r;
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
  tch.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue (ToString(BUFFER_SIZE)+"p"));
  
  QueueDiscContainer qdiscs = tch.Install (switchDevicesOut);  // in this option we installed TCH on switchDevicesOut. to send data from switch to reciever

  

  ///////// set the Traffic Controll layer to be a shared buffer////////////////////////

  Ptr<TrafficControlLayer> tc;
  tc = router.Get(0)->GetObject<TrafficControlLayer>();
  tc->SetAttribute("SharedBuffer", BooleanValue(true));
  tc->SetAttribute("MaxSharedBufferSize", StringValue (queue_capacity));
  tc->SetAttribute("Alpha_High", DoubleValue (alpha_high));
  tc->SetAttribute("Alpha_Low", DoubleValue (alpha_low));
  tc->SetAttribute("TrafficControllAlgorythm", StringValue (usedAlgorythm));

  // monitor the packets in the Shared Buffer in Traffic Control Layer:
  tc->TraceConnectWithoutContext("PacketsInQueue", MakeCallback (&TrafficControlPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("HighPriorityPacketsInQueue", MakeCallback (&TrafficControlHighPriorityPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("LowPriorityPacketsInQueue", MakeCallback (&TrafficControlLowPriorityPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_High_0", MakeBoundCallback (&TrafficControlThresholdHighTrace, 0));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_Low_0", MakeBoundCallback (&TrafficControlThresholdLowTrace, 0));  
  tc->TraceConnectWithoutContext("EnqueueingThreshold_High_1", MakeBoundCallback (&TrafficControlThresholdHighTrace, 1));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_Low_1", MakeBoundCallback (&TrafficControlThresholdLowTrace, 1));

  /////////////////////////////////////////////////////////////////////////////

  //////////////Monitor data from q-disc//////////////////////////////////////////
  for (size_t i = 0; i < RECIEVER_COUNT; i++)
  {
    Ptr<QueueDisc> queue = qdiscs.Get (i); // look at the router queue
    queue->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&QueueDiscPacketsInQueueTrace, i));
    queue->TraceConnectWithoutContext ("HighPriorityPacketsInQueue", MakeBoundCallback (&HighPriorityQueueDiscPacketsInQueueTrace, i)); 
    queue->TraceConnectWithoutContext ("LowPriorityPacketsInQueue", MakeBoundCallback (&LowPriorityQueueDiscPacketsInQueueTrace, i)); 
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

  uint32_t ipTos_LP = 0x00; //Low priority: Best Effort
  uint32_t ipTos_HP = 0x10;  //High priority: Maximize Throughput
  
  Address sinkLocalAddressP0 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P0));
  Address sinkLocalAddressP1 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P1));

  PacketSinkHelper sinkP0 (socketType, sinkLocalAddressP0); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
  PacketSinkHelper sinkP1 (socketType, sinkLocalAddressP1); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
  
  
  ApplicationContainer sinkApps, sourceApps;
  
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
    ns3::Ptr<ns3::TcpSocket> tcpsockptr =
        ns3::DynamicCast<ns3::TcpSocket> (sockptr);
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

    InetSocketAddress socketAddressP0 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P0);
    socketAddressP0.SetTos(ipTos_HP);  // ToS 0x10 -> High priority
    InetSocketAddress socketAddressP1 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P1);
    socketAddressP1.SetTos(ipTos_LP);  // ToS 0x0 -> Low priority

    // time interval values for OnOff Aplications
    // original values:
    double_t miceOnTime = 0.05; // [sec]
    double_t miceOffTime = 0.01; // [sec]
    double_t elephantOnTime = 0.5; // [sec]
    double_t elephantOffTime = 0.1; // [sec]
    // for RNG:
    double_t miceOffTimeMax = 2 * miceOffTime; // [sec]
    double_t elephantOffTimeMax = 2 * elephantOffTime; // [sec]

    // create and install Client apps:    
    if (applicationType.compare("standardClient") == 0) 
    {
      // Install UDP application on the sender 
      // send packet flows from servers with even indexes to spine 0, and from servers with odd indexes to spine 1.
      // UdpClientHelper clientHelper;
      if (i==0)
      {
        UdpClientHelper clientHelperP0 (socketAddressP0);
        clientHelperP0.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
        clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));
      }
      else if (i==1)
      {
        UdpClientHelper clientHelperP1 (socketAddressP1);
        clientHelperP1.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
        clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        sourceApps.Add(clientHelperP1.Install (servers.Get(serverIndex)));
      }
    } 
    else if (applicationType.compare("OnOff") == 0) 
    {
      // Create the OnOff applications to send TCP/UDP to the server
      if (i==0)
      {
        OnOffHelper clientHelperP0 (socketType, socketAddressP0);
        clientHelperP0.SetAttribute ("Remote", AddressValue (socketAddressP0));
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
        clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelperP0.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));
      }
      else if (i==1)
      {
        OnOffHelper clientHelperP1 (socketType, socketAddressP1);
        clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
        clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelperP1.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        sourceApps.Add(clientHelperP1.Install (servers.Get(serverIndex)));
      }
    } 
    else if (applicationType.compare("prioClient") == 0)
    {
      if (i==0)
      {
        UdpPrioClientHelper clientHelperP0 (socketAddressP0);
        clientHelperP0.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
        clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        // clientHelperP0.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelperP0.SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
        sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));
      }
      else if (i==1)
      {
        UdpPrioClientHelper clientHelperP1 (socketAddressP1);
        clientHelperP1.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
        clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        // clientHelperP1.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelperP1.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
        sourceApps.Add(clientHelperP1.Install (servers.Get(serverIndex)));
      }
    } 
    else if (applicationType.compare("prioOnOff") == 0) 
    {
      // Create the OnOff applications to send TCP/UDP to the server
      if (i==0)
      {
        PrioOnOffHelper clientHelperP0 (socketType, socketAddressP0);
        clientHelperP0.SetAttribute ("Remote", AddressValue (socketAddressP0));
        if (onoff_traffic_mode.compare("Constant") == 0)
        {
          clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
          clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
        }
        else if (onoff_traffic_mode.compare("Uniform") == 0)
        {
          clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
          clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
        }
        else if (onoff_traffic_mode.compare("Normal") == 0)
        {
          clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
          clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
        }
        else 
        {
            std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
            exit(1);
        }
        clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelperP0.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        // clientHelperP0.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelperP0.SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
        sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));
      }
      else if (i==1)
      {
        PrioOnOffHelper clientHelperP1 (socketType, socketAddressP1);
        clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
        if (onoff_traffic_mode.compare("Constant") == 0)
        {
          clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
          clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
        }
        else if (onoff_traffic_mode.compare("Uniform") == 0)
        {
          clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
          clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
        }
        else if (onoff_traffic_mode.compare("Normal") == 0)
        {
          clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
          clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
        }
        else 
        {
            std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
            exit(1);
        }
        clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelperP1.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        // clientHelperP1.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelperP1.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
        sourceApps.Add(clientHelperP1.Install (servers.Get(serverIndex)));
      }
    }
    else 
    {
        std::cerr << "unknown app type: " << applicationType << std::endl;
        exit(1);
    }
    
    // setup sink
    if (i==0)
    {
      sinkApps.Add(sinkP0.Install (recievers.Get(recieverIndex)));
    }
    else if (i==1)
    {
      sinkApps.Add(sinkP1.Install (recievers.Get(recieverIndex)));
    }       
  }

  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds(1.0 + trafficGenDuration));

  sinkApps.Start (Seconds (START_TIME));
  sinkApps.Stop (Seconds (END_TIME + 0.1));
  
  NS_LOG_INFO ("Enabling flow monitor");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  // datDir = "./Trace_Plots/test_Alphas/"
  std::string dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" + DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) + "/";
  if (!((std::filesystem::exists(dirToSave)) && (std::filesystem::is_directory(dirToSave))))
  {
    system (("mkdir -p " + dirToSave).c_str ());
  }
  
  if (eraseOldData == true)
  {
    system (("rm " + dirToSave + "/*.dat").c_str ()); // to erase the old .dat files and collect new data
    system (("rm " + dirToSave + "/*.txt").c_str ()); // to erase the previous test run summary, and collect new data
    std::cout << std::endl << "***Erased Previous Data***\n" << std::endl;
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
  // std::ofstream testFlowStatistics (dir + traffic_control_type + "/" + implementation + "/Statistics.txt", std::ios::out | std::ios::app);
  std::ofstream testFlowStatistics (dirToSave + "/Statistics.txt", std::ios::out | std::ios::app);
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

  // move all the produced .dat files to a directory based on the Alpha values
  std::string newDir = dirToSave;
  system (("mkdir -p " + newDir).c_str ());
  system (("mv -f " + datDir + "*.dat " + newDir).c_str ());
  // system (("mv -f " + datDir + "*.txt " + newDir).c_str ());

  Simulator::Destroy ();
  NS_LOG_INFO ("Stop simulation");

  // if chose to acumulate statistics:
  if (accumulate_stats)
  {
    if (!(std::filesystem::exists(dirToSave + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + usedAlgorythm + "_TestAccumulativeStatistics.dat")))
    {
      // If the file doesn't exist, create it and write initial statistics
      system (("mkdir -p " + dirToSave + "/TestStats/" + traffic_control_type + "/" + implementation + "/").c_str ());
      std::ofstream testAccumulativeStats (dirToSave + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      testAccumulativeStats
      << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
      << tcStats.nTotalDroppedPackets << " " 
      << tcStats.nTotalDroppedPacketsHighPriority << " " 
      << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
      testAccumulativeStats.close ();
    }
    else
    {
      // Open the file in append mode
      std::fstream testAccumulativeStats (dirToSave + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      testAccumulativeStats
      << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
      << tcStats.nTotalDroppedPackets << " " 
      << tcStats.nTotalDroppedPacketsHighPriority << " " 
      << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
      testAccumulativeStats.close ();
    }
  }
}


void
viaMQueues2ToS (std::string traffic_control_type, std::string onoff_traffic_mode, double_t mice_elephant_prob, double_t alpha_high, double_t alpha_low, bool accumulate_stats)
{
  LogComponentEnable ("2In2Out", LOG_LEVEL_INFO);

  std::string implementation = "via_MultiQueues/2_ToS";  // "via_NetDevices"/"via_FIFO_QueueDiscs"/"via_MultiQueues"

  std::string usedAlgorythm;  // "DT"/"FB"
  std::string applicationType = "prioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"prioOnOff"

  std::string transportProt = "UDP"; // "UDP"/"TCP"
  std::string socketType;
  std::string queue_capacity;

  bool eraseOldData = true; // true/false

  if (traffic_control_type.compare("SharedBuffer_DT") == 0)
  {
    usedAlgorythm = "DT";
  }
  else if (traffic_control_type.compare("SharedBuffer_FB") == 0)
  {
    usedAlgorythm = "FB";
  }

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
  // for loop use. make sure name "Router" is not stored in Names map///
  Names::Clear();
  /////////////////////////////////////////////////////////
  Names::Add("Router", router.Get(0));  // Add a Name to the router node
  NodeContainer servers;
  servers.Create (SERVER_COUNT);


  // NS_LOG_INFO ("Install channels and assign addresses");

  PointToPointHelper n2s, s2r;
  NS_LOG_INFO ("Configuring channels for all the Nodes");
  // Setting servers
  uint64_t serverSwitchCapacity = 2 * SERVER_SWITCH_CAPACITY;
  n2s.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (serverSwitchCapacity)));
  n2s.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
  n2s.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));  // set basic queues to 1 packet
  // setting routers
  uint64_t switchRecieverCapacity = 2 * SWITCH_RECIEVER_CAPACITY;
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
  std::array<uint16_t, 16> prioArray = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  
  // Priomap prioMap = Priomap{prioArray};
  TosMap tosMap = TosMap{prioArray};
  uint16_t handle = tch.SetRootQueueDisc("ns3::RoundRobinTosQueueDisc", "TosMap", TosMapValue(tosMap));

  TrafficControlHelper::ClassIdList cid = tch.AddQueueDiscClasses(handle, 2, "ns3::QueueDiscClass");
  tch.AddChildQueueDisc(handle, cid[0], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[0] is band "0" - the Highest Priority band
  tch.AddChildQueueDisc(handle, cid[1], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[1] is < cid[0]

  QueueDiscContainer qdiscs = tch.Install (switchDevicesOut);  // in this option we installed TCH on switchDevicesOut. to send data from switch to reciever

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

  /////////////////////////////////////////////////////////////////////////////
    // std::cout << "current time: "  << Simulator::Now() << std::endl;
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

  uint32_t ipTos_HP = 0x10;  //High priority: Maximize Throughput
  uint32_t ipTos_LP1 = 0x00; //(Low) priority 0: Best Effort
  
  
  ApplicationContainer sinkApps, sourceApps;
  
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
    ns3::Ptr<ns3::TcpSocket> tcpsockptr =
        ns3::DynamicCast<ns3::TcpSocket> (sockptr);
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
    
    InetSocketAddress socketAddressP0 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P0);
    socketAddressP0.SetTos(ipTos_HP);   // ToS 0x10 -> High priority
    InetSocketAddress socketAddressP1 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P1);
    socketAddressP1.SetTos(ipTos_LP1);  // ToS 0x00 -> Low priority


    // time interval values for OnOff Aplications
    // original values:
    double_t miceOnTime = 0.05; // [sec]
    double_t miceOffTime = 0.01; // [sec]
    double_t elephantOnTime = 0.5; // [sec]
    double_t elephantOffTime = 0.1; // [sec]

    // for RNG:
    double_t miceOffTimeMax = 2 * miceOffTime; // [sec]
    double_t elephantOffTimeMax = 2 * elephantOffTime; // [sec]
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
      clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
      clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
      clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP0.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));

      OnOffHelper clientHelperP1 (socketType, socketAddressP1);
      clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
      clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
      clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
      clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP1.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
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
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
      }
      else 
      {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
      }
      clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP0.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      // clientHelperP0.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP0.SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));

      PrioOnOffHelper clientHelperP1 (socketType, socketAddressP1);
      clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
      if (onoff_traffic_mode.compare("Constant") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
      }
      else 
      {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
      }
      clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP1.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      // clientHelperP1.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP1.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP1.Install (servers.Get(serverIndex)));
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
  
  // double_t trafficGenDuration = 2; // initilize for a single OnOff segment
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds(1.0 + trafficGenDuration));

  sinkApps.Start (Seconds (START_TIME));
  sinkApps.Stop (Seconds (END_TIME + 0.1));
  
  NS_LOG_INFO ("Enabling flow monitor");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  // datDir = "./Trace_Plots/test_Alphas/"
  std::string dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" + DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) + "/";
  if (!((std::filesystem::exists(dirToSave)) && (std::filesystem::is_directory(dirToSave))))
  {
    system (("mkdir -p " + dirToSave).c_str ());
  }
  
  if (eraseOldData == true)
  {
    system (("rm " + dirToSave + "/*.dat").c_str ()); // to erase the old .dat files and collect new data
    system (("rm " + dirToSave + "/*.txt").c_str ()); // to erase the previous test run summary, and collect new data
    std::cout << std::endl << "***Erased Previous Data***\n" << std::endl;
  }

  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (END_TIME + 10)); // original
  // Simulator::Stop (Seconds (6));  // doesn't work
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl << "Topology: 2In2Out" << std::endl;
  std::cout << std::endl << "Queueing Algorithm: " + traffic_control_type << std::endl;
  std::cout << std::endl << "Implementation Method: " + implementation << std::endl;
  std::cout << std::endl << "Used D value: " + DoubleToString(mice_elephant_prob) << std::endl;
  std::cout << std::endl << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
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
  std::ofstream testFlowStatistics (dirToSave + "/Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: 2In2Out" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + traffic_control_type << std::endl;
  testFlowStatistics << "Implementation Method: " + implementation << std::endl;
  testFlowStatistics << "Used D value: " + DoubleToString(mice_elephant_prob) << std::endl;
  testFlowStatistics << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
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
  std::string newDir = dirToSave;
  system (("mkdir -p " + newDir).c_str ());
  system (("mv -f " + datDir + "*.dat " + newDir).c_str ());
  // system (("mv -f " + datDir + "*.txt " + newDir).c_str ());

  // if chose to acumulate statistics:
  if (accumulate_stats)
  {
    if (!(std::filesystem::exists(datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                  + usedAlgorythm + "_TestAccumulativeStatistics.dat")))
    {
      // If the file doesn't exist, create it and write initial statistics
      system (("mkdir -p " + datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/").c_str ());
      std::ofstream testAccumulativeStats (datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                            + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      testAccumulativeStats
      << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
      << tcStats.nTotalDroppedPackets << " " 
      << tcStats.nTotalDroppedPacketsHighPriority << " " 
      << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
      testAccumulativeStats.close ();
    }
    else
    {
      // Open the file in append mode
      std::fstream testAccumulativeStats (datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                          + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      testAccumulativeStats
      << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
      << tcStats.nTotalDroppedPackets << " " 
      << tcStats.nTotalDroppedPacketsHighPriority << " " 
      << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
      testAccumulativeStats.close ();
    }
  }
  
  // clear all statistics for this simulation itteration
  flowStats.clear();
  // delete tcStats;  doesn't work!
  tcStats.reset();
  tcStats.~TCStats();

  // Simulator::Cancel();
  Simulator::Destroy ();
  NS_LOG_INFO ("Stop simulation");
}

template <size_t N>
void
viaMQueues2ToSVaryingD (std::string traffic_control_type, std::string onoff_traffic_mode, const std::double_t(&mice_elephant_prob_array)[N], double_t alpha_high, double_t alpha_low, bool adjustableAlphas, bool accumulate_stats)
{
  LogComponentEnable ("2In2Out", LOG_LEVEL_INFO);

  std::string implementation = "via_MultiQueues/2_ToS";  // "via_NetDevices"/"via_FIFO_QueueDiscs"/"via_MultiQueues"

  std::string usedAlgorythm;  // "DT"/"FB"
  std::string applicationType = "prioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"prioOnOff"

  std::string transportProt = "UDP"; // "UDP"/"TCP"
  std::string socketType;
  std::string queue_capacity;

  if (traffic_control_type.compare("SharedBuffer_DT") == 0)
  {
      usedAlgorythm = "DT";
  }
  else if (traffic_control_type.compare("SharedBuffer_FB") == 0)
  {
      usedAlgorythm = "FB";
  }

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
  // for loop use. make sure name "Router" is not stored in Names map///
  Names::Clear();
  /////////////////////////////////////////////////////////
  Names::Add("Router", router.Get(0));  // Add a Name to the router node
  NodeContainer servers;
  servers.Create (SERVER_COUNT);


  // NS_LOG_INFO ("Install channels and assign addresses");

  PointToPointHelper n2s, s2r;
  NS_LOG_INFO ("Configuring channels for all the Nodes");
  // Setting servers
  uint64_t serverSwitchCapacity = 2 * SERVER_SWITCH_CAPACITY;
  n2s.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (serverSwitchCapacity)));
  n2s.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
  n2s.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));  // set basic queues to 1 packet
  // setting routers
  uint64_t switchRecieverCapacity = 2 * SWITCH_RECIEVER_CAPACITY;
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
  std::array<uint16_t, 16> prioArray = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  
  // Priomap prioMap = Priomap{prioArray};
  TosMap tosMap = TosMap{prioArray};
  uint16_t handle = tch.SetRootQueueDisc("ns3::RoundRobinTosQueueDisc", "TosMap", TosMapValue(tosMap));

  TrafficControlHelper::ClassIdList cid = tch.AddQueueDiscClasses(handle, 2, "ns3::QueueDiscClass");
  tch.AddChildQueueDisc(handle, cid[0], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[0] is band "0" - the Highest Priority band
  tch.AddChildQueueDisc(handle, cid[1], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[1] is < cid[0]

  QueueDiscContainer qdiscs = tch.Install (switchDevicesOut);  // in this option we installed TCH on switchDevicesOut. to send data from switch to reciever

  

  ///////// set the Traffic Controll layer to be a shared buffer////////////////////////
  TcPriomap tcPrioMap = TcPriomap{prioArray};
  Ptr<TrafficControlLayer> tc;
  tc = router.Get(0)->GetObject<TrafficControlLayer>();
  tc->SetAttribute("SharedBuffer", BooleanValue(true));
  tc->SetAttribute("MaxSharedBufferSize", StringValue (queue_capacity));
  tc->SetAttribute("Alpha_High", DoubleValue (alpha_high));
  tc->SetAttribute("Alpha_Low", DoubleValue (alpha_low));
  tc->SetAttribute("TrafficControllAlgorythm", StringValue (usedAlgorythm));
  tc->SetAttribute("AdjustableAlphas", BooleanValue(adjustableAlphas));
  tc->SetAttribute("PriorityMapforMultiQueue", TcPriomapValue(tcPrioMap));

  // monitor the packets in the Shared Buffer in Traffic Control Layer:
  tc->TraceConnectWithoutContext("PacketsInQueue", MakeCallback (&TrafficControlPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("HighPriorityPacketsInQueue", MakeCallback (&TrafficControlHighPriorityPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("LowPriorityPacketsInQueue", MakeCallback (&TrafficControlLowPriorityPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_High_0", MakeBoundCallback (&TrafficControlThresholdHighTrace, 0));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_Low_0", MakeBoundCallback (&TrafficControlThresholdLowTrace, 0));  
  tc->TraceConnectWithoutContext("EnqueueingThreshold_High_1", MakeBoundCallback (&TrafficControlThresholdHighTrace, 1));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_Low_1", MakeBoundCallback (&TrafficControlThresholdLowTrace, 1));

  /////////////////////////////////////////////////////////////////////////////
    // std::cout << "current time: "  << Simulator::Now() << std::endl;
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

  uint32_t ipTos_HP = 0x10;  //High priority: Maximize Throughput
  uint32_t ipTos_LP1 = 0x00; //(Low) priority 0: Best Effort
  
  ApplicationContainer sinkApps;
  std::vector<ApplicationContainer> sourceApps_vector;
  // create and install Client apps:  
  for (size_t i = 0; i < N; i++) // iterate over all D values to create SourceAppContainers Array
  {
    ApplicationContainer sourceApps;
    sourceApps_vector.push_back(sourceApps);
  }
  
  for (size_t i = 0; i < 2; i++)  // iterate over switch ports
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
    ns3::Ptr<ns3::TcpSocket> tcpsockptr =
        ns3::DynamicCast<ns3::TcpSocket> (sockptr);
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
    
    InetSocketAddress socketAddressP0 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P0);
    socketAddressP0.SetTos(ipTos_HP);   // ToS 0x10 -> High priority
    InetSocketAddress socketAddressP1 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P1);
    socketAddressP1.SetTos(ipTos_LP1);  // ToS 0x00 -> Low priority


    // time interval values for OnOff Aplications
    // std::string miceOnTime = "0.05"; // [sec]
    // std::string elephantOnTime = "0.5"; // [sec]
    
    if (applicationType.compare("prioOnOff") == 0) 
    {
      std::vector<PrioOnOffHelper> clientHelpersP0_vector;
      std::vector<PrioOnOffHelper> clientHelpersP1_vector;
      // create and install Client apps:  
      for (size_t j = 0; j < N; j++) // iterate over all D values to create SourceApps Array for each port
      {
        PrioOnOffHelper clientHelperP0(socketType, socketAddressP0);
        PrioOnOffHelper clientHelperP1(socketType, socketAddressP1);

        clientHelpersP0_vector.push_back(clientHelperP0);
        clientHelpersP1_vector.push_back(clientHelperP1);
      }
      
      for (size_t j = 0; j < N; j++) // iterate over all D values to configure and install OnOff Application for each Client
      {
        double_t mice_elephant_prob = mice_elephant_prob_array[j];
        // original values:
        double_t miceOffTime = 0.01; // [sec]
        double_t elephantOffTime = 0.1; // [sec]
        // for RNG:
        double_t miceOffTimeMax = 2 * miceOffTime; // [sec]
        double_t elephantOffTimeMax = 2 * elephantOffTime; // [sec]

        clientHelpersP0_vector[j].SetAttribute ("Remote", AddressValue (socketAddressP0));
        if (onoff_traffic_mode.compare("Constant") == 0)
        {
          clientHelpersP0_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
          clientHelpersP0_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
        }
        else if (onoff_traffic_mode.compare("Uniform") == 0)
        {
          clientHelpersP0_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
          clientHelpersP0_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
        }
        else if (onoff_traffic_mode.compare("Normal") == 0)
        {
          clientHelpersP0_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
          clientHelpersP0_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
        }
        else 
        {
            std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
            exit(1);
        }
        clientHelpersP0_vector[j].SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelpersP0_vector[j].SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        // clientHelpersP0_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelpersP0_vector[j].SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
        clientHelpersP0_vector[j].SetAttribute("MiceElephantProbability", StringValue (DoubleToString(mice_elephant_prob)));
        sourceApps_vector[j].Add(clientHelpersP0_vector[j].Install (servers.Get(serverIndex)));

        clientHelpersP1_vector[j].SetAttribute ("Remote", AddressValue (socketAddressP1));
        if (onoff_traffic_mode.compare("Constant") == 0)
        {
          clientHelpersP1_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
          clientHelpersP1_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
        }
        else if (onoff_traffic_mode.compare("Uniform") == 0)
        {
          clientHelpersP1_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
          clientHelpersP1_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
        }
        else if (onoff_traffic_mode.compare("Normal") == 0)
        {
          clientHelpersP1_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
          clientHelpersP1_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
        }
        else 
        {
            std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
            exit(1);
        }
        clientHelpersP1_vector[j].SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelpersP1_vector[j].SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        // clientHelpersP1_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelpersP1_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
        clientHelpersP1_vector[j].SetAttribute("MiceElephantProbability", StringValue (DoubleToString(mice_elephant_prob)));
        sourceApps_vector[j].Add(clientHelpersP1_vector[j].Install (servers.Get(serverIndex)));
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
    PacketSinkHelper sinkP0 (socketType, sinkLocalAddressP0); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    PacketSinkHelper sinkP1 (socketType, sinkLocalAddressP1); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    sinkApps.Add(sinkP0.Install (recievers.Get(recieverIndex)));
    sinkApps.Add(sinkP1.Install (recievers.Get(recieverIndex)));    
  }

  // double_t trafficGenDuration = 2; // for a single OnOff segment
  double_t silenceDuration = 4; // time between OnOff segments

  for (size_t i = 0; i < N; i++) // iterate over all D values to synchronize Start/Stop for all OnOffApps
  {
    sourceApps_vector[i].Start (Seconds (1.0 + i * (trafficGenDuration + silenceDuration)));
    sourceApps_vector[i].Stop (Seconds (1.0 + (i + 1) * trafficGenDuration + i * silenceDuration));
  }
  

  sinkApps.Start (Seconds (START_TIME));
  sinkApps.Stop (Seconds (END_TIME + 0.1));
  
  NS_LOG_INFO ("Enabling flow monitor");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  // datDir = "./Trace_Plots/test_Alphas/"
  std::string dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/";
  if (!((std::filesystem::exists(dirToSave)) && (std::filesystem::is_directory(dirToSave))))
  {
    system (("mkdir -p " + dirToSave).c_str ());
  }

  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (END_TIME + 10)); 
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl << "Topology: 2In2Out" << std::endl;
  std::cout << std::endl << "Queueing Algorithm: " + traffic_control_type << std::endl;
  std::cout << std::endl << "Implementation Method: " + implementation << std::endl;
  std::cout << std::endl << "Used D value: [" + DoubleToString(mice_elephant_prob_array[0]) + ", " +
                                                DoubleToString(mice_elephant_prob_array[1]) + ", " + 
                                                DoubleToString(mice_elephant_prob_array[2]) + "]" << std::endl;
  if (adjustableAlphas)
  {
    std::cout << std::endl << "Adjustable Alpha High/Low values" << std::endl;
  }
  else
  {
    std::cout << std::endl << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low << std::endl;
  }
  std::cout << std::endl << "Traffic Duration: 3x" + DoubleToString(trafficGenDuration) + " [Sec]" << std::endl;
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
  std::ofstream testFlowStatistics (dirToSave + "Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: 2In2Out" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + traffic_control_type << std::endl;
  testFlowStatistics << "Implementation Method: " + implementation << std::endl;
  testFlowStatistics << "Used D value: [" + DoubleToString(mice_elephant_prob_array[0]) + ", " +
                                            DoubleToString(mice_elephant_prob_array[1]) + ", " + 
                                            DoubleToString(mice_elephant_prob_array[2]) + "]" << std::endl;
  if (adjustableAlphas)
  {
    testFlowStatistics << "Adjustable Alpha High/Low values" << std::endl;
  }
  else
  {
    testFlowStatistics << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
  }
  testFlowStatistics << std::endl << "Traffic Duration: 3x" + DoubleToString(trafficGenDuration) + " [Sec]" << std::endl;
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

  if (adjustableAlphas)
  {
    // move all the produced files to a directory
    // datDir = "./Trace_Plots/test_Alphas/"
    // std::string dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/";

    std::string newDir = dirToSave + "VaryingDValues/adjustableAlphas/";
    system (("mkdir -p " + newDir).c_str ());
    system (("mv -f " + datDir + "/*.dat " + newDir).c_str ());
    system (("mv -f " + dirToSave + "/*.txt " + newDir).c_str ());
  }
  else
  {
    // move all the produced files to a directory based on the Alpha values
    std::string newDir = dirToSave + "VaryingDValues/" + DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) + "/";
    system (("mkdir -p " + newDir).c_str ());
    system (("mv -f " + datDir + "/*.dat " + newDir).c_str ());
    system (("mv -f " + dirToSave + "/*.txt " + newDir).c_str ());
  }

  // if choose to acumulate statistics:
  if (accumulate_stats)
  {
    if (!(std::filesystem::exists(datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/VaryingDValues/" 
                                  + usedAlgorythm + "_TestAccumulativeStatistics.dat")))
    {
      // If the file doesn't exist, create it and write initial statistics
      system (("mkdir -p " + datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/VaryingDValues/").c_str ());
      std::ofstream testAccumulativeStats (datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/VaryingDValues/" 
                                            + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      if (adjustableAlphas)
      {
        testAccumulativeStats
        << "AdjustableAlphas" << " "
        << tcStats.nTotalDroppedPackets << " " 
        << tcStats.nTotalDroppedPacketsHighPriority << " " 
        << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
        testAccumulativeStats.close ();
      }
      else
      {
        testAccumulativeStats
        << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
        << tcStats.nTotalDroppedPackets << " " 
        << tcStats.nTotalDroppedPacketsHighPriority << " " 
        << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
        testAccumulativeStats.close ();
      }
    }
    else
    {
      // Open the file in append mode
      std::fstream testAccumulativeStats (datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/VaryingDValues/" 
                                          + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      if (adjustableAlphas)
      {
        testAccumulativeStats
        << "AdjustableAlphas" << " "
        << tcStats.nTotalDroppedPackets << " " 
        << tcStats.nTotalDroppedPacketsHighPriority << " " 
        << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
        testAccumulativeStats.close ();
      }
      else
      {
        testAccumulativeStats
        << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
        << tcStats.nTotalDroppedPackets << " " 
        << tcStats.nTotalDroppedPacketsHighPriority << " " 
        << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
        testAccumulativeStats.close ();
      }
    }
  }
  
  // clear all statistics for this simulation itteration
  flowStats.clear();
  tcStats.reset();
  tcStats.~TCStats();

  Simulator::Destroy ();
  NS_LOG_INFO ("Stop simulation");
}

void
viaMQueuesPredictive2ToS (std::string traffic_control_type, std::string onoff_traffic_mode, double_t mice_elephant_prob, bool accumulate_stats)
{
  LogComponentEnable ("2In2Out", LOG_LEVEL_INFO);

  std::string implementation = "via_MultiQueues/2_ToS";  // "via_NetDevices"/"via_FIFO_QueueDiscs"/"via_MultiQueues"

  std::string usedAlgorythm;  // "DT"/"FB"/"PredictiveFB"
  std::string applicationType = "prioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"prioOnOff"

  std::string transportProt = "UDP"; // "UDP"/"TCP"
  std::string socketType;
  std::string queue_capacity;

  bool eraseOldData = true; // true/false

  if (traffic_control_type.compare("SharedBuffer_DT") == 0)
  {
      usedAlgorythm = "DT";
  }
  else if (traffic_control_type.compare("SharedBuffer_FB") == 0)
  {
      usedAlgorythm = "FB";
  }
  else if (traffic_control_type.compare("SharedBuffer_PredictiveFB") == 0)
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

  PointToPointHelper n2s, s2r, n2sPredict, s2rPredict;
  NS_LOG_INFO ("Configuring channels for all the Nodes");

  // Setting servers
  uint64_t serverSwitchCapacity = 2 * SERVER_SWITCH_CAPACITY;
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
  std::array<uint16_t, 16> prioArray = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  
  // Priomap prioMap = Priomap{prioArray};
  TosMap tosMap = TosMap{prioArray};
  uint16_t handle = tch.SetRootQueueDisc("ns3::RoundRobinTosQueueDisc", "TosMap", TosMapValue(tosMap));

  TrafficControlHelper::ClassIdList cid = tch.AddQueueDiscClasses(handle, 2, "ns3::QueueDiscClass");
  tch.AddChildQueueDisc(handle, cid[0], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[0] is band "0" - the Highest Priority band
  tch.AddChildQueueDisc(handle, cid[1], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[1] is < cid[0]

  QueueDiscContainer qdiscs = tch.Install (switchDevicesOut);  // in this option we installed TCH on switchDevicesOut. to send data from switch to reciever
  // for predictive model Install Traffic Control Helper on the Predictive model
  QueueDiscContainer qdiscsPredict = tch.Install(switchDevicesOutPredict);

  ///////// set the Traffic Controll layer to be a shared buffer////////////////////////
  TcPriomap tcPrioMap = TcPriomap{prioArray};
  Ptr<TrafficControlLayer> tc;
  tc = router.Get(0)->GetObject<TrafficControlLayer>();
  tc->SetAttribute("SharedBuffer", BooleanValue(true));
  tc->SetAttribute("MaxSharedBufferSize", StringValue (queue_capacity));
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
  tcPredict->SetAttribute("PriorityMapforMultiQueue", TcPriomapValue(tcPrioMap));

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
  uint32_t ipTos_LP1 = 0x00; //(Low) priority 0: Best Effort
  
  
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
    socketAddressP0.SetTos(ipTos_HP);   // ToS 0x10 -> High priority
    InetSocketAddress socketAddressP1 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P1);
    socketAddressP1.SetTos(ipTos_LP1);  // ToS 0x00 -> Low priority

    // for preddictive model
    InetSocketAddress socketAddressP0Predict = InetSocketAddress (recieverIFsPredict.GetAddress(recieverIndex), SERV_PORT_P0);
    socketAddressP0Predict.SetTos(ipTos_HP);  // ToS 0x10 -> High priority
    InetSocketAddress socketAddressP1Predict = InetSocketAddress (recieverIFsPredict.GetAddress(recieverIndex), SERV_PORT_P1);
    socketAddressP1Predict.SetTos(ipTos_LP1);  // ToS 0x0 -> Low priority

    // time interval values for OnOff Aplications
    // original values:
    double_t miceOnTime = 0.05; // [sec]
    double_t miceOffTime = 0.01; // [sec]
    double_t elephantOnTime = 0.5; // [sec]
    double_t elephantOffTime = 0.1; // [sec]
    // for RNG:
    double_t miceOffTimeMax = 2 * miceOffTime; // [sec]
    double_t elephantOffTimeMax = 2 * elephantOffTime; // [sec]
    
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
      clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
      clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
      clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP0.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));

      OnOffHelper clientHelperP1 (socketType, socketAddressP1);
      clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
      clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
      clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
      clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP1.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
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
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
      }
      else 
      {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
      }
      clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP0.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      // clientHelperP0.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP0.SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));
      clientHelperP0.AssignStreams(servers.Get(serverIndex), 0);

      PrioOnOffHelper clientHelperP1 (socketType, socketAddressP1);
      clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
      if (onoff_traffic_mode.compare("Constant") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
      }
      else 
      {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
      }
      clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP1.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      // clientHelperP1.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP1.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP1.Install (servers.Get(serverIndex)));
      clientHelperP1.AssignStreams(servers.Get(serverIndex), 0);

      // for predicting traffic in queue
      PrioOnOffHelper clientHelperP0Predict (socketType, socketAddressP0Predict);
      clientHelperP0Predict.SetAttribute ("Remote", AddressValue (socketAddressP0Predict));
      if (onoff_traffic_mode.compare("Constant") == 0)
      {
        clientHelperP0Predict.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
        clientHelperP0Predict.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP0Predict.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
        clientHelperP0Predict.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP0Predict.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
        clientHelperP0Predict.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
      }
      else 
      {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
      }
      clientHelperP0Predict.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP0Predict.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      // clientHelperP0.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP0Predict.SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceAppsPredict.Add(clientHelperP0Predict.Install (serversPredict.Get(serverIndex)));
      clientHelperP0Predict.AssignStreams(serversPredict.Get(serverIndex), 0);

      PrioOnOffHelper clientHelperP1Predict (socketType, socketAddressP1Predict);
      clientHelperP1Predict.SetAttribute ("Remote", AddressValue (socketAddressP1Predict));
      if (onoff_traffic_mode.compare("Constant") == 0)
      {
        clientHelperP1Predict.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP1Predict.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP1Predict.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
        clientHelperP1Predict.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP1Predict.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP1Predict.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
      }
      else 
      {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
      }
      clientHelperP1Predict.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP1Predict.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      // clientHelperP1Predict.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP1Predict.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceAppsPredict.Add(clientHelperP1Predict.Install (serversPredict.Get(serverIndex)));
      clientHelperP1Predict.AssignStreams(serversPredict.Get(serverIndex), 0);
    }
    else 
    {
      std::cerr << "unknown app type: " << applicationType << std::endl;
      exit(1);
    }
    // assign streams for RNG:
    // clientHelperP0.AssignStreams(servers, 1);
    // clientHelperP1.AssignStreams(servers, 1);
    // clientHelperP0Predict.AssignStreams(serversPredict, 1);
    // clientHelperP1Predict.AssignStreams(serversPredict, 1);

    // setup sinks
    Address sinkLocalAddressP0 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P0));
    Address sinkLocalAddressP1 (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT_P1));
    PacketSinkHelper sinkP0 (socketType, sinkLocalAddressP0); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    PacketSinkHelper sinkP1 (socketType, sinkLocalAddressP1); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    sinkApps.Add(sinkP0.Install (recievers.Get(recieverIndex)));
    sinkApps.Add(sinkP1.Install (recievers.Get(recieverIndex)));    
  }
  
  // double_t trafficGenDuration = 2; // initilize for a single OnOff segment
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds(1.0 + trafficGenDuration));
  // Tau = 100 [msec]. start predictive model at t0 - Tau
  double_t tau = 0.1;
  sourceAppsPredict.Start (Seconds (1.0 - tau));
  sourceAppsPredict.Stop (Seconds(1.0 + trafficGenDuration - tau));

  sinkApps.Start (Seconds (START_TIME));
  sinkApps.Stop (Seconds (END_TIME + 0.1));
  
  NS_LOG_INFO ("Enabling flow monitor");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  // datDir = "./Trace_Plots/test_Alphas/"
  std::string dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/";
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
  std::cout << std::endl << "Queueing Algorithm: " + traffic_control_type << std::endl;
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
  std::ofstream testFlowStatistics (dirToSave + "Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: 2In2Out" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + traffic_control_type << std::endl;
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
  // dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/";
  std::string newDir = dirToSave;
  system (("mkdir -p " + newDir).c_str ());
  system (("mv -f " + datDir + "*.dat " + newDir).c_str ());
  // system (("mv -f " + datDir + "*.txt " + newDir).c_str ());

  // if chose to acumulate statistics:
  if (accumulate_stats)
  {
    if (!(std::filesystem::exists(datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                  + usedAlgorythm + "_TestAccumulativeStatistics.dat")))
    {
      // If the file doesn't exist, create it and write initial statistics
      system (("mkdir -p " + datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/").c_str ());
      std::ofstream testAccumulativeStats (datDir  + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
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
      std::fstream testAccumulativeStats (datDir  + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
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
  tcStats.reset();
  tcStats.~TCStats();

  // Simulator::Cancel();
  Simulator::Destroy ();
  NS_LOG_INFO ("Stop simulation");
}

void
viaMQueues5ToS (std::string traffic_control_type, std::string onoff_traffic_mode, double_t mice_elephant_prob, double_t alpha_high, double_t alpha_low, bool accumulate_stats)
{
  LogComponentEnable ("2In2Out", LOG_LEVEL_INFO);

  std::string implementation = "via_MultiQueues/5_ToS";
  std::string usedAlgorythm;
  std::string applicationType = "prioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"prioOnOff"
  std::string transportProt = "UDP"; // "UDP"/"TCP"
  std::string socketType;
  std::string queue_capacity;
  bool eraseOldData = true; // true/false
  if (traffic_control_type.compare("SharedBuffer_DT") == 0)
  {
    usedAlgorythm = "DT";
  }
  else if (traffic_control_type.compare("SharedBuffer_FB") == 0)
  {
    usedAlgorythm = "FB";
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
  /////////////////////////////////////////////////////////
  Names::Add("Router", router.Get(0));  // Add a Name to the router node
  NodeContainer servers;
  servers.Create (SERVER_COUNT);


  // NS_LOG_INFO ("Install channels and assign addresses");

  PointToPointHelper n2s, s2r;
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

  /////////////////////////////////////////////////////////////////////////////

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

  uint32_t ipTos_HP = 0x10;  //High priority: Maximize Throughput
  uint32_t ipTos_LP1 = 0x00; //(Low) priority 0: Best Effort
  uint32_t ipTos_LP2 = 0x02; //(Low) priority 0: Best Effort
  uint32_t ipTos_LP3 = 0x04; //(Low) priority 0: Best Effort
  uint32_t ipTos_LP4 = 0x06; //(Low) priority 0: Best Effort
  
  
  ApplicationContainer sinkApps, sourceApps;
  
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
    ns3::Ptr<ns3::TcpSocket> tcpsockptr =
        ns3::DynamicCast<ns3::TcpSocket> (sockptr);
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
    
    InetSocketAddress socketAddressP0 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P0);
    socketAddressP0.SetTos(ipTos_HP);   // ToS 0x10 -> High priority
    InetSocketAddress socketAddressP1 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P1);
    socketAddressP1.SetTos(ipTos_LP1);  // ToS 0x00 -> Low priority
    InetSocketAddress socketAddressP2 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P2);
    socketAddressP2.SetTos(ipTos_LP2);  // ToS 0x02 -> Low priority
    InetSocketAddress socketAddressP3 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P3);
    socketAddressP3.SetTos(ipTos_LP3);  // ToS 0x04 -> Low priority
    InetSocketAddress socketAddressP4 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P4);
    socketAddressP4.SetTos(ipTos_LP4);  // ToS 0x06 -> Low priority

    // // time interval values for OnOff Aplications
    double_t miceOffTime = (1 - mice_elephant_prob) * 2 * miceOffTimeConst; // [sec]
    double_t elephantOffTime = mice_elephant_prob * 2 * 4 * elephantOffTimeConst; // [sec]
    // for debug:
    std::cout << "miceOffTime: " << miceOffTime << "[sec]" << std::endl;
    std::cout << "elephantOffTime: " << elephantOffTime << "[sec]" << std::endl;
    // for RNG:
    double_t miceOffTimeMax = 2 * miceOffTime; // [sec]
    double_t elephantOffTimeMax = 2 * elephantOffTime; // [sec]

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
      clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
      clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
      clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP0.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));

      OnOffHelper clientHelperP1 (socketType, socketAddressP1);
      clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
      clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
      clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
      clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP1.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
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
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
        clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
      }
      else 
      {
        std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
        exit(1);
      }
      clientHelperP0.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP0.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      // clientHelperP0.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP0.SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP0.Install (servers.Get(serverIndex)));

      PrioOnOffHelper clientHelperP1 (socketType, socketAddressP1);
      clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
      if (onoff_traffic_mode.compare("Constant") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
      }
      else 
      {
        std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
        exit(1);
      }
      clientHelperP1.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP1.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      // clientHelperP1.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP1.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP1.Install (servers.Get(serverIndex)));

      PrioOnOffHelper clientHelperP2 (socketType, socketAddressP2);
      clientHelperP2.SetAttribute ("Remote", AddressValue (socketAddressP2));
      if (onoff_traffic_mode.compare("Constant") == 0)
      {
        clientHelperP2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP2.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
        clientHelperP2.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP2.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP2.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
      }
      else 
      {
        std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
        exit(1);
      }
      clientHelperP2.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP2.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      // clientHelperP2.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP2.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP2.Install (servers.Get(serverIndex)));

      PrioOnOffHelper clientHelperP3 (socketType, socketAddressP3);
      clientHelperP3.SetAttribute ("Remote", AddressValue (socketAddressP3));
      if (onoff_traffic_mode.compare("Constant") == 0)
      {
        clientHelperP3.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP3.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP3.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
        clientHelperP3.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP3.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP3.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
      }
      else 
      {
        std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
        exit(1);
      }
      clientHelperP3.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP3.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      // clientHelperP3.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP3.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP3.Install (servers.Get(serverIndex)));

      PrioOnOffHelper clientHelperP4 (socketType, socketAddressP4);
      clientHelperP4.SetAttribute ("Remote", AddressValue (socketAddressP4));
      if (onoff_traffic_mode.compare("Constant") == 0)
      {
        clientHelperP4.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP4.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
      }
      else if (onoff_traffic_mode.compare("Uniform") == 0)
      {
        clientHelperP4.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
        clientHelperP4.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
      }
      else if (onoff_traffic_mode.compare("Normal") == 0)
      {
        clientHelperP4.SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
        clientHelperP4.SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
      }
      else 
      {
        std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
        exit(1);
      }
      clientHelperP4.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
      clientHelperP4.SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
      // clientHelperP4.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
      clientHelperP4.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
      sourceApps.Add(clientHelperP4.Install (servers.Get(serverIndex)));
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

  sinkApps.Start (Seconds (START_TIME));
  sinkApps.Stop (Seconds (END_TIME + 0.1));
  
  NS_LOG_INFO ("Enabling flow monitor");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  std::string dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" + DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) + "/";
  if (!((std::filesystem::exists(dirToSave)) && (std::filesystem::is_directory(dirToSave))))
  {
    system (("mkdir -p " + dirToSave).c_str ());
  }
  
  if (eraseOldData == true)
  {
    system (("rm " + dirToSave + "/*.dat").c_str ()); // to erase the old .dat files and collect new data
    system (("rm " + dirToSave + "/*.txt").c_str ()); // to erase the previous test run summary, and collect new data
    std::cout << std::endl << "***Erased Previous Data***\n" << std::endl;
  }

  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (END_TIME + 10));
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl << "Topology: 2In2Out" << std::endl;
  std::cout << std::endl << "Queueing Algorithm: " + traffic_control_type << std::endl;
  std::cout << std::endl << "Implementation Method: " + implementation << std::endl;
  std::cout << std::endl << "Used D value: " + DoubleToString(mice_elephant_prob) << std::endl;
  std::cout << std::endl << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
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
  std::ofstream testFlowStatistics (dirToSave + "/Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: 2In2Out" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + traffic_control_type << std::endl;
  testFlowStatistics << "Implementation Method: " + implementation << std::endl;
  testFlowStatistics << "Used D value: " + DoubleToString(mice_elephant_prob) << std::endl;
  testFlowStatistics << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
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
  std::string newDir = dirToSave;
  system (("mkdir -p " + newDir).c_str ());
  system (("mv -f " + datDir + "*.dat " + newDir).c_str ());
  // system (("mv -f " + datDir + "*.txt " + newDir).c_str ());

  // if chose to acumulate statistics:
  if (accumulate_stats)
  {
    if (!(std::filesystem::exists(datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                  + usedAlgorythm + "_TestAccumulativeStatistics.dat")))
    {
      // If the file doesn't exist, create it and write initial statistics
      system (("mkdir -p " + datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/").c_str ());
      std::ofstream testAccumulativeStats (datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                            + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      testAccumulativeStats
      << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
      << tcStats.nTotalDroppedPackets << " " 
      << tcStats.nTotalDroppedPacketsHighPriority << " " 
      << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
      testAccumulativeStats.close ();
    }
    else
    {
      // Open the file in append mode
      std::fstream testAccumulativeStats (datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                          + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      testAccumulativeStats
      << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
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

void
viaMQueues5ToS_v2 (std::string traffic_control_type, std::string onoff_traffic_mode, double_t mice_elephant_prob, double_t alpha_high, double_t alpha_low, bool accumulate_stats)
{
  LogComponentEnable ("2In2Out", LOG_LEVEL_INFO);

  std::string implementation = "via_MultiQueues/5_ToS";
  std::string usedAlgorythm;
  std::string applicationType = "prioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"prioOnOff"
  std::string transportProt = "UDP"; // "UDP"/"TCP"
  std::string socketType;
  std::string queue_capacity;
  bool eraseOldData = true; // true/false
  if (traffic_control_type.compare("SharedBuffer_DT") == 0)
  {
    usedAlgorythm = "DT";
  }
  else if (traffic_control_type.compare("SharedBuffer_FB") == 0)
  {
    usedAlgorythm = "FB";
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
  /////////////////////////////////////////////////////////
  Names::Add("Router", router.Get(0));  // Add a Name to the router node
  NodeContainer servers;
  servers.Create (SERVER_COUNT);


  // NS_LOG_INFO ("Install channels and assign addresses");

  PointToPointHelper n2s, s2r;
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
  double_t Num_M_High = mice_elephant_prob * 10; // total number of OnOff machines that generate High priority traffic
  TcPriomap tcPrioMap = TcPriomap{prioArray};
  Ptr<TrafficControlLayer> tc;
  tc = router.Get(0)->GetObject<TrafficControlLayer>();
  tc->SetAttribute("SharedBuffer", BooleanValue(true));
  tc->SetAttribute("MaxSharedBufferSize", StringValue (queue_capacity));
  tc->SetAttribute("Alpha_High", DoubleValue (alpha_high));
  tc->SetAttribute("Alpha_Low", DoubleValue (alpha_low));
  tc->SetAttribute("NumberOfHighProbabilityOnOffMachines", DoubleValue (Num_M_High));
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

  /////////////////////////////////////////////////////////////////////////////

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
  
  
  ApplicationContainer sinkApps, sourceApps;

  // time interval values for OnOff Aplications
  double_t miceOffTime = 0.01; // [sec]
  double_t elephantOffTime = 0.1; // [sec]
  // for RNG:
  double_t miceOffTimeMax = 2 * miceOffTime; // [sec]
  double_t elephantOffTimeMax = 2 * elephantOffTime; // [sec]

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
    ns3::Ptr<ns3::Socket> sockptr;

    if (transportProt.compare("TCP") == 0) 
    {
    // setup source socket
    sockptr =
        ns3::Socket::CreateSocket(servers.Get(serverIndex),
                ns3::TcpSocketFactory::GetTypeId());
    ns3::Ptr<ns3::TcpSocket> tcpsockptr =
        ns3::DynamicCast<ns3::TcpSocket> (sockptr);
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
          if (i == 0 && j < int(ceil(Num_M_High/2)) || i == 1 && j < int(floor(Num_M_High/2)))
          {
            if (onoff_traffic_mode.compare("Constant") == 0)
            { 
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
            }
            else if (onoff_traffic_mode.compare("Uniform") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
            }
            else if (onoff_traffic_mode.compare("Normal") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
              // test:
              // clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTimeMax) + "]"));
              // clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTimeMax) +"]"));
              // clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "]"));
              // clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) + "]"));
            }
            else if (onoff_traffic_mode.compare("Exponential") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ExponentialRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(trafficGenDuration) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(trafficGenDuration) +"]"));
            }
            else 
            {
              std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
              exit(1);
            }
            clientHelpers_vector[j].SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
          }
          else
          {
            if (onoff_traffic_mode.compare("Constant") == 0)
            { 
            clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
            clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
            }
            else if (onoff_traffic_mode.compare("Uniform") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(elephantOnTimeMax) +"]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
            }
            else if (onoff_traffic_mode.compare("Normal") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(elephantOffTime) +"]"));
              // test:
              // clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTimeMax) + "]"));
              // clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(elephantOffTimeMax) +"]"));
              // clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "]"));
              // clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) + "]"));
            }
            else if (onoff_traffic_mode.compare("Exponential") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ExponentialRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(trafficGenDuration) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(trafficGenDuration) +"]"));
            }
            else 
            {
              std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
              exit(1);
            }
            clientHelpers_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low 
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
            }
            else if (onoff_traffic_mode.compare("Uniform") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
            }
            else if (onoff_traffic_mode.compare("Normal") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
              // test:
              // clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTimeMax) + "]"));
              // clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTimeMax) +"]"));
              // clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "]"));
              // clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) + "]"));
            }
            else if (onoff_traffic_mode.compare("Exponential") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ExponentialRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(trafficGenDuration) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(trafficGenDuration) +"]"));
            }
            else 
            {
              std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
              exit(1);
            }
            clientHelpers_vector[j].SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
          }
          else // create OnOff machines that generate Low priority traffic
          {
            if (onoff_traffic_mode.compare("Constant") == 0)
            { 
            clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
            clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
            }
            else if (onoff_traffic_mode.compare("Uniform") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(elephantOnTimeMax) +"]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
            }
            else if (onoff_traffic_mode.compare("Normal") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(elephantOffTime) +"]"));
              // test:
              // clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTimeMax) + "]"));
              // clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(elephantOffTimeMax) +"]"));
              // clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "]"));
              // clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) + "]"));
            }
            else if (onoff_traffic_mode.compare("Exponential") == 0)
            {
              clientHelpers_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ExponentialRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(trafficGenDuration) + "]"));
              clientHelpers_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(trafficGenDuration) +"]"));
            }
            else 
            {
              std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
              exit(1);
            }
            clientHelpers_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low  
          }
        }
        clientHelpers_vector[j].SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelpers_vector[j].SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        // clientHelpers_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelpers_vector[j].SetAttribute("MiceElephantProbability", StringValue (DoubleToString(mice_elephant_prob)));
        sourceApps.Add(clientHelpers_vector[j].Install (servers.Get(serverIndex)));
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

  sinkApps.Start (Seconds (START_TIME));
  sinkApps.Stop (Seconds (END_TIME + 0.1));
  
  NS_LOG_INFO ("Enabling flow monitor");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  // std::string dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" + DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) + "/";
  std::string dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/";
  if (!((std::filesystem::exists(dirToSave)) && (std::filesystem::is_directory(dirToSave))))
  {
    system (("mkdir -p " + dirToSave).c_str ());
  }
  
  if (eraseOldData == true)
  {
    system (("rm " + dirToSave + "/*.dat").c_str ()); // to erase the old .dat files and collect new data
    system (("rm " + dirToSave + "/*.txt").c_str ()); // to erase the previous test run summary, and collect new data
    std::cout << std::endl << "***Erased Previous Data***\n" << std::endl;
  }

  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (END_TIME + 10));
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl << "Topology: 2In2Out" << std::endl;
  std::cout << std::endl << "Queueing Algorithm: " + traffic_control_type << std::endl;
  std::cout << std::endl << "Implementation Method: " + implementation << std::endl;
  std::cout << std::endl << "Used D value: " + DoubleToString(mice_elephant_prob) << std::endl;
  std::cout << std::endl << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
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
  std::ofstream testFlowStatistics (dirToSave + "/Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: 2In2Out" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + traffic_control_type << std::endl;
  testFlowStatistics << "Implementation Method: " + implementation << std::endl;
  testFlowStatistics << "Used D value: " + DoubleToString(mice_elephant_prob) << std::endl;
  testFlowStatistics << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
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
  // dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/";
  std::string newDir = dirToSave + DoubleToString(mice_elephant_prob) + "/" + DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) + "/";
  // std::string newDir = dirToSave;
  system (("mkdir -p " + newDir).c_str ());
  system (("mv -f " + datDir + "*.dat " + newDir).c_str ());
  system (("mv -f " + dirToSave + "*.txt " + newDir).c_str ());

  // if chose to acumulate statistics:
  if (accumulate_stats)
  {
    if (!(std::filesystem::exists(datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                  + usedAlgorythm + "_TestAccumulativeStatistics.dat")))
    {
      // If the file doesn't exist, create it and write initial statistics
      system (("mkdir -p " + datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/").c_str ());
      std::ofstream testAccumulativeStats (datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                            + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      testAccumulativeStats
      << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
      << tcStats.nTotalDroppedPackets << " " 
      << tcStats.nTotalDroppedPacketsHighPriority << " " 
      << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
      testAccumulativeStats.close ();
    }
    else
    {
      // Open the file in append mode
      std::fstream testAccumulativeStats (datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" + DoubleToString(mice_elephant_prob) + "/" 
                                          + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      testAccumulativeStats
      << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
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

template <size_t N>
void
viaMQueues5ToSVaryingD (std::string traffic_control_type,std::string onoff_traffic_mode, const std::double_t(&mice_elephant_prob_array)[N], double_t alpha_high, double_t alpha_low, bool adjustableAlphas, bool accumulate_stats)
{
  LogComponentEnable ("2In2Out", LOG_LEVEL_INFO);

  std::string implementation = "via_MultiQueues/5_ToS/VaryingDValues";
  std::string usedAlgorythm;
  std::string applicationType = "prioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"prioOnOff"
  std::string transportProt = "UDP"; // "UDP"/"TCP"
  std::string socketType;
  std::string queue_capacity;

  if (traffic_control_type.compare("SharedBuffer_DT") == 0)
  {
    usedAlgorythm = "DT";
  }
  else if (traffic_control_type.compare("SharedBuffer_FB") == 0)
  {
    usedAlgorythm = "FB";
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
  /////////////////////////////////////////////////////////
  Names::Add("Router", router.Get(0));  // Add a Name to the router node
  NodeContainer servers;
  servers.Create (SERVER_COUNT);


  // NS_LOG_INFO ("Install channels and assign addresses");

  PointToPointHelper n2s, s2r;
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
  tc->SetAttribute("Alpha_High", DoubleValue (alpha_high));
  tc->SetAttribute("Alpha_Low", DoubleValue (alpha_low));
  tc->SetAttribute("TrafficControllAlgorythm", StringValue (usedAlgorythm));
  tc->SetAttribute("AdjustableAlphas", BooleanValue(adjustableAlphas));
  tc->SetAttribute("PriorityMapforMultiQueue", TcPriomapValue(tcPrioMap));

  // monitor the packets in the Shared Buffer in Traffic Control Layer:
  tc->TraceConnectWithoutContext("PacketsInQueue", MakeCallback (&TrafficControlPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("HighPriorityPacketsInQueue", MakeCallback (&TrafficControlHighPriorityPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("LowPriorityPacketsInQueue", MakeCallback (&TrafficControlLowPriorityPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_High_0", MakeBoundCallback (&TrafficControlThresholdHighTrace, 0));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_Low_0", MakeBoundCallback (&TrafficControlThresholdLowTrace, 0));  
  tc->TraceConnectWithoutContext("EnqueueingThreshold_High_1", MakeBoundCallback (&TrafficControlThresholdHighTrace, 1));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_Low_1", MakeBoundCallback (&TrafficControlThresholdLowTrace, 1));

  /////////////////////////////////////////////////////////////////////////////

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

  uint32_t ipTos_HP = 0x10;  //High priority: Maximize Throughput
  uint32_t ipTos_LP1 = 0x00; //(Low) priority 0: Best Effort
  uint32_t ipTos_LP2 = 0x02; //(Low) priority 0: Best Effort
  uint32_t ipTos_LP3 = 0x04; //(Low) priority 0: Best Effort
  uint32_t ipTos_LP4 = 0x06; //(Low) priority 0: Best Effort
  
  
  ApplicationContainer sinkApps;
  std::vector<ApplicationContainer> sourceApps_vector;
  // create and install Client apps:  
  for (size_t i = 0; i < N; i++) // iterate over all D values to create SourceAppContainers Array
  {
    ApplicationContainer sourceApps;
    sourceApps_vector.push_back(sourceApps);
  }
  
  for (size_t i = 0; i < 2; i++) // iterate over switch ports
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
    ns3::Ptr<ns3::TcpSocket> tcpsockptr =
        ns3::DynamicCast<ns3::TcpSocket> (sockptr);
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
    
    InetSocketAddress socketAddressP0 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P0);
    socketAddressP0.SetTos(ipTos_HP);   // ToS 0x10 -> High priority
    InetSocketAddress socketAddressP1 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P1);
    socketAddressP1.SetTos(ipTos_LP1);  // ToS 0x00 -> Low priority
    InetSocketAddress socketAddressP2 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P2);
    socketAddressP2.SetTos(ipTos_LP2);  // ToS 0x02 -> Low priority
    InetSocketAddress socketAddressP3 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P3);
    socketAddressP3.SetTos(ipTos_LP3);  // ToS 0x04 -> Low priority
    InetSocketAddress socketAddressP4 = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT_P4);
    socketAddressP4.SetTos(ipTos_LP4);  // ToS 0x06 -> Low priority
    
    // create and install Client apps:    
    if (applicationType.compare("prioOnOff") == 0) 
    {
      // Create the OnOff applications to send TCP/UDP to the server
      std::vector<PrioOnOffHelper> clientHelpersP0_vector;
      std::vector<PrioOnOffHelper> clientHelpersP1_vector;
      std::vector<PrioOnOffHelper> clientHelpersP2_vector;
      std::vector<PrioOnOffHelper> clientHelpersP3_vector;
      std::vector<PrioOnOffHelper> clientHelpersP4_vector; 
      for (size_t j = 0; j < N; j++) // iterate over all D values to create SourceApps Array for each port
      {
        PrioOnOffHelper clientHelperP0(socketType, socketAddressP0);
        PrioOnOffHelper clientHelperP1(socketType, socketAddressP1);
        PrioOnOffHelper clientHelperP2(socketType, socketAddressP2);
        PrioOnOffHelper clientHelperP3(socketType, socketAddressP3);
        PrioOnOffHelper clientHelperP4(socketType, socketAddressP4);

        clientHelpersP0_vector.push_back(clientHelperP0);
        clientHelpersP1_vector.push_back(clientHelperP1);
        clientHelpersP2_vector.push_back(clientHelperP2);
        clientHelpersP3_vector.push_back(clientHelperP3);
        clientHelpersP4_vector.push_back(clientHelperP4);
      }
      
      for (size_t j = 0; j < N; j++) // iterate over all D values to configure and install OnOff Application for each Client
      {
        double_t mice_elephant_prob = mice_elephant_prob_array[j];
        // in this simulation it effects silence time ratio for the onoff application
        // std::string miceOffTime = DoubleToString((1 - mice_elephant_prob) * 2 * (1/4) * miceOffTimeConst); // [sec]
        // double_t elephantOffTime = mice_elephant_prob * 2 * elephantOffTimeConst; // [sec]
        double_t miceOffTime = (1 - mice_elephant_prob) * 2 * miceOffTimeConst; // [sec]
        double_t elephantOffTime = mice_elephant_prob * 2 * 4 * elephantOffTimeConst; // [sec]
        // for RNG:
        double_t miceOffTimeMax = 2 * miceOffTime; // [sec]
        double_t elephantOffTimeMax = 2 * elephantOffTime; // [sec]

        clientHelpersP0_vector[j].SetAttribute ("Remote", AddressValue (socketAddressP0));
        if (onoff_traffic_mode.compare("Constant") == 0)
        {
          clientHelpersP0_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
          clientHelpersP0_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
        }
        else if (onoff_traffic_mode.compare("Uniform") == 0)
        {
          clientHelpersP0_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
          clientHelpersP0_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
        }
        else if (onoff_traffic_mode.compare("Normal") == 0)
        {
          clientHelpersP0_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
          clientHelpersP0_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
        }
        else 
        {
            std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
            exit(1);
        }
        clientHelpersP0_vector[j].SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelpersP0_vector[j].SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        // clientHelpersP0_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelpersP0_vector[j].SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
        clientHelpersP0_vector[j].SetAttribute("MiceElephantProbability", StringValue (DoubleToString(mice_elephant_prob)));
        sourceApps_vector[j].Add(clientHelpersP0_vector[j].Install (servers.Get(serverIndex)));

        clientHelpersP1_vector[j].SetAttribute ("Remote", AddressValue (socketAddressP1));
        if (onoff_traffic_mode.compare("Constant") == 0)
        {
          clientHelpersP1_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
          clientHelpersP1_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
        }
        else if (onoff_traffic_mode.compare("Uniform") == 0)
        {
          clientHelpersP1_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
          clientHelpersP1_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
        }
        else if (onoff_traffic_mode.compare("Normal") == 0)
        {
          clientHelpersP1_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
          clientHelpersP1_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
        }
        else 
        {
            std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
            exit(1);
        }
        clientHelpersP1_vector[j].SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelpersP1_vector[j].SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        // clientHelpersP1_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelpersP1_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
        clientHelpersP1_vector[j].SetAttribute("MiceElephantProbability", StringValue (DoubleToString(mice_elephant_prob)));
        sourceApps_vector[j].Add(clientHelpersP1_vector[j].Install (servers.Get(serverIndex)));

        clientHelpersP2_vector[j].SetAttribute ("Remote", AddressValue (socketAddressP2));
        if (onoff_traffic_mode.compare("Constant") == 0)
        {
          clientHelpersP2_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
          clientHelpersP2_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
        }
        else if (onoff_traffic_mode.compare("Uniform") == 0)
        {
          clientHelpersP2_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
          clientHelpersP2_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
        }
        else if (onoff_traffic_mode.compare("Normal") == 0)
        {
          clientHelpersP2_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
          clientHelpersP2_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
        }
        else 
        {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
        }
        clientHelpersP2_vector[j].SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelpersP2_vector[j].SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        // clientHelpersP2_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelpersP2_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
        clientHelpersP2_vector[j].SetAttribute("MiceElephantProbability", StringValue (DoubleToString(mice_elephant_prob)));
        sourceApps_vector[j].Add(clientHelpersP2_vector[j].Install (servers.Get(serverIndex)));

        clientHelpersP3_vector[j].SetAttribute ("Remote", AddressValue (socketAddressP3));
        if (onoff_traffic_mode.compare("Constant") == 0)
        {
          clientHelpersP3_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
          clientHelpersP3_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
        }
        else if (onoff_traffic_mode.compare("Uniform") == 0)
        {
          clientHelpersP3_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
          clientHelpersP3_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
        }
        else if (onoff_traffic_mode.compare("Normal") == 0)
        {
          clientHelpersP3_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
          clientHelpersP3_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
        }
        else 
        {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
        }
        clientHelpersP3_vector[j].SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelpersP3_vector[j].SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        // clientHelpersP3_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelpersP3_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
        clientHelpersP3_vector[j].SetAttribute("MiceElephantProbability", StringValue (DoubleToString(mice_elephant_prob)));
        sourceApps_vector[j].Add(clientHelpersP3_vector[j].Install (servers.Get(serverIndex)));

        clientHelpersP4_vector[j].SetAttribute ("Remote", AddressValue (socketAddressP4));
        if (onoff_traffic_mode.compare("Constant") == 0)
        {
          clientHelpersP4_vector[j].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
          clientHelpersP4_vector[j].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
        }
        else if (onoff_traffic_mode.compare("Uniform") == 0)
        {
          clientHelpersP4_vector[j].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOnTimeMax) + "]"));
          clientHelpersP4_vector[j].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
        }
        else if (onoff_traffic_mode.compare("Normal") == 0)
        {
          clientHelpersP4_vector[j].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
          clientHelpersP4_vector[j].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOffTime) + "|Variance=" + DoubleToString(elephantOffTime) + "|Bound=" + DoubleToString(elephantOffTime) + "]"));
        }
        else 
        {
          std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
          exit(1);
        }
        clientHelpersP4_vector[j].SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
        clientHelpersP4_vector[j].SetAttribute ("DataRate", StringValue (IntToString(dataRate) + "Mb/s"));
        // clientHelpersP4_vector[j].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
        clientHelpersP4_vector[j].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
        clientHelpersP4_vector[j].SetAttribute("MiceElephantProbability", StringValue (DoubleToString(mice_elephant_prob)));
        sourceApps_vector[j].Add(clientHelpersP4_vector[j].Install (servers.Get(serverIndex)));
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
  double_t silenceDuration = 10; // time between OnOff segments

  for (size_t i = 0; i < N; i++) // iterate over all D values to synchronize Start/Stop for all OnOffApps
  {
    sourceApps_vector[i].Start (Seconds (1.0 + i * (trafficGenDuration + silenceDuration)));
    sourceApps_vector[i].Stop (Seconds (1.0 + (i + 1) * trafficGenDuration + i * silenceDuration));
  }

  sinkApps.Start (Seconds (START_TIME));
  sinkApps.Stop (Seconds (END_TIME + 0.1));
  
  NS_LOG_INFO ("Enabling flow monitor");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  // datDir = "./Trace_Plots/test_Alphas/"
  std::string dirToSave = datDir + traffic_control_type + "/" + onoff_traffic_mode + "/" + implementation + "/";
  if (!((std::filesystem::exists(dirToSave)) && (std::filesystem::is_directory(dirToSave))))
  {
    system (("mkdir -p " + dirToSave).c_str ());
  }

  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (END_TIME + 10));
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl << "Topology: 2In2Out" << std::endl;
  std::cout << std::endl << "Queueing Algorithm: " + traffic_control_type << std::endl;
  std::cout << std::endl << "Implementation Method: " + implementation << std::endl;
  std::cout << std::endl << "Used D value: [" + DoubleToString(mice_elephant_prob_array[0]) + ", " +
                                                DoubleToString(mice_elephant_prob_array[1]) + ", " + 
                                                DoubleToString(mice_elephant_prob_array[2]) + "]" << std::endl;
  if (adjustableAlphas)
  {
    std::cout << std::endl << "Adjustable Alpha High/Low values" << std::endl;
  }
  else
  {
    std::cout << std::endl << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low << std::endl;
  }
  std::cout << std::endl << "Traffic Duration: 3x" + DoubleToString(trafficGenDuration) + " [Sec]" << std::endl;
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
  std::ofstream testFlowStatistics (dirToSave + "/Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: 2In2Out" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + traffic_control_type << std::endl;
  testFlowStatistics << "Implementation Method: " + implementation << std::endl;
  testFlowStatistics << "Used D value: [" + DoubleToString(mice_elephant_prob_array[0]) + ", " +
                                            DoubleToString(mice_elephant_prob_array[1]) + ", " + 
                                            DoubleToString(mice_elephant_prob_array[2]) + "]" << std::endl;
  if (adjustableAlphas)
  {
    testFlowStatistics << "Adjustable Alpha High/Low values" << std::endl;
  }
  else
  {
    testFlowStatistics << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
  }
  testFlowStatistics << "Traffic Duration: 3x" + DoubleToString(trafficGenDuration) + " [Sec]" << std::endl;
  testFlowStatistics << "Application: " + applicationType << std::endl;
  testFlowStatistics << "Traffic Mode " + onoff_traffic_mode + "Random" << std::endl; 
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

  if (adjustableAlphas)
  {
    // move all the produced files to a directory
    // dirToSave = datDir + traffic_control_type + "/" + implementation + "/"
    std::string newDir = dirToSave + "adjustableAlphas/";
    system (("mkdir -p " + newDir).c_str ());
    system (("mv -f " + datDir + "/*.dat " + newDir).c_str ());
    system (("mv -f " + dirToSave + "/*.txt " + newDir).c_str ());
  }
  else
  {
    // move all the produced files to a directory based on the Alpha values
    std::string newDir = dirToSave + DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) + "/";
    system (("mkdir -p " + newDir).c_str ());
    system (("mv -f " + datDir + "/*.dat " + newDir).c_str ());
    system (("mv -f " + dirToSave + "/*.txt " + newDir).c_str ());
  }

// if choose to acumulate statistics:
  if (accumulate_stats)
  {
    if (!(std::filesystem::exists(datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" 
                                  + usedAlgorythm + "_TestAccumulativeStatistics.dat")))
    {
      // If the file doesn't exist, create it and write initial statistics
      system (("mkdir -p " + datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/").c_str ());
      std::ofstream testAccumulativeStats (datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" 
                                            + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      if (adjustableAlphas)
      {
        testAccumulativeStats
        << "AdjustableAlphas" << " "
        << tcStats.nTotalDroppedPackets << " " 
        << tcStats.nTotalDroppedPacketsHighPriority << " " 
        << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
        testAccumulativeStats.close ();
      }
      else
      {
        testAccumulativeStats
        << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
        << tcStats.nTotalDroppedPackets << " " 
        << tcStats.nTotalDroppedPacketsHighPriority << " " 
        << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
        testAccumulativeStats.close ();
      }
    }
    else
    {
      // Open the file in append mode
      std::fstream testAccumulativeStats (datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/" 
                                          + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      if (adjustableAlphas)
      {
        testAccumulativeStats
        << "AdjustableAlphas" << " "
        << tcStats.nTotalDroppedPackets << " " 
        << tcStats.nTotalDroppedPacketsHighPriority << " " 
        << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
        testAccumulativeStats.close ();
      }
      else
      {
        testAccumulativeStats
        << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
        << tcStats.nTotalDroppedPackets << " " 
        << tcStats.nTotalDroppedPacketsHighPriority << " " 
        << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
        testAccumulativeStats.close ();
      }
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

template <size_t N>
void
viaMQueues5ToS_v2_VaryingD (std::string traffic_control_type,std::string onoff_traffic_mode, const std::double_t(&mice_elephant_prob_array)[N], double_t alpha_high, double_t alpha_low, bool adjustableAlphas, bool accumulate_stats)
{
  LogComponentEnable ("2In2Out", LOG_LEVEL_INFO);

  std::string implementation = "via_MultiQueues/5_ToS";
  std::string usedAlgorythm;
  std::string applicationType = "prioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"prioOnOff"
  std::string transportProt = "UDP"; // "UDP"/"TCP"
  std::string socketType;
  std::string queue_capacity;
  bool eraseOldData = true; // true/false
  if (traffic_control_type.compare("SharedBuffer_DT") == 0)
  {
    usedAlgorythm = "DT";
  }
  else if (traffic_control_type.compare("SharedBuffer_FB") == 0)
  {
    usedAlgorythm = "FB";
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
  /////////////////////////////////////////////////////////
  Names::Add("Router", router.Get(0));  // Add a Name to the router node
  NodeContainer servers;
  servers.Create (SERVER_COUNT);


  // NS_LOG_INFO ("Install channels and assign addresses");

  PointToPointHelper n2s, s2r;
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
  tc->SetAttribute("Alpha_High", DoubleValue (alpha_high));
  tc->SetAttribute("Alpha_Low", DoubleValue (alpha_low));
  tc->SetAttribute("TrafficControllAlgorythm", StringValue (usedAlgorythm));
  tc->SetAttribute("AdjustableAlphas", BooleanValue(adjustableAlphas));
  tc->SetAttribute("PriorityMapforMultiQueue", TcPriomapValue(tcPrioMap));

  // monitor the packets in the Shared Buffer in Traffic Control Layer:
  tc->TraceConnectWithoutContext("PacketsInQueue", MakeCallback (&TrafficControlPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("HighPriorityPacketsInQueue", MakeCallback (&TrafficControlHighPriorityPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("LowPriorityPacketsInQueue", MakeCallback (&TrafficControlLowPriorityPacketsInSharedQueueTrace));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_High_0", MakeBoundCallback (&TrafficControlThresholdHighTrace, 0));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_Low_0", MakeBoundCallback (&TrafficControlThresholdLowTrace, 0));  
  tc->TraceConnectWithoutContext("EnqueueingThreshold_High_1", MakeBoundCallback (&TrafficControlThresholdHighTrace, 1));
  tc->TraceConnectWithoutContext("EnqueueingThreshold_Low_1", MakeBoundCallback (&TrafficControlThresholdLowTrace, 1));

  /////////////////////////////////////////////////////////////////////////////

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
  
  
  ApplicationContainer sinkApps;
  std::vector<ApplicationContainer> sourceApps_vector;
  // create and install Client apps:  
  for (size_t i = 0; i < N; i++) // iterate over all D values to create SourceAppContainers Array
  {
    ApplicationContainer sourceApps;
    sourceApps_vector.push_back(sourceApps);
  }
  
  // time interval values for OnOff Aplications
  // double_t miceOnTime = 0.05; // [sec]
  // double_t elephantOnTime = 0.5; // [sec]
  double_t miceOffTime = 0.01; // [sec]
  double_t elephantOffTime = 0.1; // [sec]
  // for RNG:
  // double_t miceOnTimeMax = 0.1; // [sec]
  // double_t elephantOnTimeMax = 1.0; // [sec]
  double_t miceOffTimeMax = 2 * miceOffTime; // [sec]
  double_t elephantOffTimeMax = 2 * elephantOffTime; // [sec]

  std::vector<double_t> Num_M_High_vector;  
  std::vector<bool> unequalNum_vector;  
  for (size_t i = 0; i < N; i++) // iterate over all D values to create SourceAppContainers Array
  {
    double_t Num_M_High = mice_elephant_prob_array[i] * 10;  // number of OnOff machines that generate High priority traffic
    Num_M_High_vector.push_back(Num_M_High);
    bool unequalNum = false;  // a flag that's raised if the number of High/Low priority OnOff applications created is diffeent for each Port
    if ((int(Num_M_High) % 2) != 0)
    {
      unequalNum = true;  
    }
    unequalNum_vector.push_back(unequalNum);
  }


  for (size_t i = 0; i < 2; i++) // iterate over switch ports
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
    ns3::Ptr<ns3::TcpSocket> tcpsockptr =
        ns3::DynamicCast<ns3::TcpSocket> (sockptr);
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
    
    std::vector<std::vector<PrioOnOffHelper>> clientHelpers_matrix;
    for (size_t j = 0; j < N; j++) // iterate over all D values to configure and install OnOff Application for each Client
    {
      clientHelpers_matrix.push_back(clientHelpers_vector);
      // create and install Client apps:    
      if (applicationType.compare("prioOnOff") == 0) 
      {
        for (size_t k = 0; k < 5; k++) 
        {
          clientHelpers_matrix[j][k].SetAttribute ("Remote", AddressValue (socketAddress_vector[k]));
          if (unequalNum_vector[j]) 
          {
            // in case the number of High/Low priority OnOff applications is not equally devidable by 2, 
            // divide the High/Low priority OnOff applications between the 2 servers such that:
            // 1st server recives the higher number of High priority and lower number of Low priority OnOff applications
            // and the 2nd server recives lower number of High priority and higher number of Low priority OnOff applications
            if (i == 0 && k < int(ceil(Num_M_High_vector[j]/2)) || i == 1 && k < int(floor(Num_M_High_vector[j]/2)))
            {
              if (onoff_traffic_mode.compare("Constant") == 0)
              { 
                clientHelpers_matrix[j][k].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
                clientHelpers_matrix[j][k].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
              }
              else if (onoff_traffic_mode.compare("Uniform") == 0)
              {
                clientHelpers_matrix[j][k].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
                clientHelpers_matrix[j][k].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
              }
              else if (onoff_traffic_mode.compare("Normal") == 0)
              {
                clientHelpers_matrix[j][k].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
                clientHelpers_matrix[j][k].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
              }
              else 
              {
                std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
                exit(1);
              }
              clientHelpers_matrix[j][k].SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
            }
            else
            {
              if (onoff_traffic_mode.compare("Constant") == 0)
              { 
              clientHelpers_matrix[j][k].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpers_matrix[j][k].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
              }
              else if (onoff_traffic_mode.compare("Uniform") == 0)
              {
                clientHelpers_matrix[j][k].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(elephantOnTimeMax) +"]"));
                clientHelpers_matrix[j][k].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
              }
              else if (onoff_traffic_mode.compare("Normal") == 0)
              {
                clientHelpers_matrix[j][k].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
                clientHelpers_matrix[j][k].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(elephantOffTime) +"]"));
              }
              else 
              {
                std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
                exit(1);
              }
              clientHelpers_matrix[j][k].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low 
            }
          }
          else
          {
            if (k < int(Num_M_High_vector[j]/2)) // create OnOff machines that generate High priority traffic
            {
              if (onoff_traffic_mode.compare("Constant") == 0)
              { 
                clientHelpers_matrix[j][k].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(miceOnTime) + "]"));
                clientHelpers_matrix[j][k].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant="+ DoubleToString(miceOffTime) +"]"));
              }
              else if (onoff_traffic_mode.compare("Uniform") == 0)
              {
                clientHelpers_matrix[j][k].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(miceOnTimeMax) +"]"));
                clientHelpers_matrix[j][k].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(miceOffTimeMax) + "]"));
              }
              else if (onoff_traffic_mode.compare("Normal") == 0)
              {
                clientHelpers_matrix[j][k].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(miceOnTime) + "|Variance=" + DoubleToString(miceOnTime) + "|Bound=" + DoubleToString(miceOnTime) + "]"));
                clientHelpers_matrix[j][k].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(miceOffTime) +"|Variance="+ DoubleToString(miceOffTime) +"|Bound="+ DoubleToString(miceOffTime) +"]"));
              }
              else 
              {
                std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
                exit(1);
              }
              clientHelpers_matrix[j][k].SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
            }
            else // create OnOff machines that generate Low priority traffic
            {
              if (onoff_traffic_mode.compare("Constant") == 0)
              { 
              clientHelpers_matrix[j][k].SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOnTime) + "]"));
              clientHelpers_matrix[j][k].SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + DoubleToString(elephantOffTime) + "]"));
              }
              else if (onoff_traffic_mode.compare("Uniform") == 0)
              {
                clientHelpers_matrix[j][k].SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max="+ DoubleToString(elephantOnTimeMax) +"]"));
                clientHelpers_matrix[j][k].SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=" + DoubleToString(elephantOffTimeMax) + "]"));
              }
              else if (onoff_traffic_mode.compare("Normal") == 0)
              {
                clientHelpers_matrix[j][k].SetAttribute ("OnTime", StringValue ("ns3::NormalRandomVariable[Mean=" + DoubleToString(elephantOnTime) + "|Variance=" + DoubleToString(elephantOnTime) + "|Bound=" + DoubleToString(elephantOnTime) + "]"));
                clientHelpers_matrix[j][k].SetAttribute ("OffTime", StringValue ("ns3::NormalRandomVariable[Mean="+ DoubleToString(elephantOffTime) +"|Variance="+ DoubleToString(elephantOffTime) +"|Bound="+ DoubleToString(elephantOffTime) +"]"));
              }
              else 
              {
                std::cerr << "unknown OnOffMode type: " << onoff_traffic_mode << std::endl;
                exit(1);
              }
              clientHelpers_matrix[j][k].SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low  
            }
          }
          clientHelpers_matrix[j][k].SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
          clientHelpers_matrix[j][k].SetAttribute ("DataRate", StringValue ("2Mb/s"));
          // clientHelpers_matrix[j][k].SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
          clientHelpers_matrix[j][k].SetAttribute("MiceElephantProbability", StringValue (DoubleToString(mice_elephant_prob_array[j])));
          sourceApps_vector[j].Add(clientHelpers_matrix[j][k].Install (servers.Get(serverIndex)));
        }
      }
      else 
      {
        std::cerr << "unknown app type: " << applicationType << std::endl;
        exit(1);
      }
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
  double_t silenceDuration = 10; // time between OnOff segments

  for (size_t i = 0; i < N; i++) // iterate over all D values to synchronize Start/Stop for all OnOffApps
  {
    sourceApps_vector[i].Start (Seconds (1.0 + i * (trafficGenDuration + silenceDuration)));
    sourceApps_vector[i].Stop (Seconds (1.0 + (i + 1) * trafficGenDuration + i * silenceDuration));
  }

  sinkApps.Start (Seconds (START_TIME));
  sinkApps.Stop (Seconds (END_TIME + 0.1));
  
  NS_LOG_INFO ("Enabling flow monitor");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  // datDir = "./Trace_Plots/test_Alphas/"
  std::string dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/";
  if (!((std::filesystem::exists(dirToSave)) && (std::filesystem::is_directory(dirToSave))))
  {
    system (("mkdir -p " + dirToSave).c_str ());
  }

  if (eraseOldData == true)
  {
    system (("rm " + dirToSave + "/*.dat").c_str ()); // to erase the old .dat files and collect new data
    system (("rm " + dirToSave + "/*.txt").c_str ()); // to erase the previous test run summary, and collect new data
    std::cout << std::endl << "***Erased Previous Data***\n" << std::endl;
  }

  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (END_TIME + 10));
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl << "Topology: 2In2Out" << std::endl;
  std::cout << std::endl << "Queueing Algorithm: " + traffic_control_type << std::endl;
  std::cout << std::endl << "Implementation Method: " + implementation << std::endl;
  std::cout << std::endl << "Used D value: [" + DoubleToString(mice_elephant_prob_array[0]) + ", " +
                                                DoubleToString(mice_elephant_prob_array[1]) + ", " + 
                                                DoubleToString(mice_elephant_prob_array[2]) + "]" << std::endl;
  if (adjustableAlphas)
  {
    std::cout << std::endl << "Adjustable Alpha High/Low values" << std::endl;
  }
  else
  {
    std::cout << std::endl << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low << std::endl;
  }
  std::cout << std::endl << "Traffic Duration: 3x" + DoubleToString(trafficGenDuration) + " [Sec]" << std::endl;
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
  std::ofstream testFlowStatistics (dirToSave + "Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: 2In2Out" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + traffic_control_type << std::endl;
  testFlowStatistics << "Implementation Method: " + implementation << std::endl;
  testFlowStatistics << "Used D value: [" + DoubleToString(mice_elephant_prob_array[0]) + ", " +
                                            DoubleToString(mice_elephant_prob_array[1]) + ", " + 
                                            DoubleToString(mice_elephant_prob_array[2]) + "]" << std::endl;
  if (adjustableAlphas)
  {
    testFlowStatistics << "Adjustable Alpha High/Low values" << std::endl;
  }
  else
  {
    testFlowStatistics << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
  }
  testFlowStatistics << "Traffic Duration: 3x" + DoubleToString(trafficGenDuration) + " [Sec]" << std::endl;
  testFlowStatistics << "Application: " + applicationType << std::endl;
  testFlowStatistics << "Traffic Mode " + onoff_traffic_mode + "Random" << std::endl; 
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

  if (adjustableAlphas)
  {
    // move all the produced files to a directory
    // datDir = "./Trace_Plots/test_Alphas/"
    // dirToSave = datDir + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/";

    std::string newDir = dirToSave + "VaryingDValues/adjustableAlphas/";
    system (("mkdir -p " + newDir).c_str ());
    system (("mv -f " + datDir + "/*.dat " + newDir).c_str ());
    system (("mv -f " + dirToSave + "/*.txt " + newDir).c_str ());
  }
  else
  {
    // move all the produced files to a directory based on the Alpha values
    std::string newDir = dirToSave + "VaryingDValues/" + DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) + "/";
    system (("mkdir -p " + newDir).c_str ());
    system (("mv -f " + datDir + "/*.dat " + newDir).c_str ());
    system (("mv -f " + dirToSave + "/*.txt " + newDir).c_str ());
  }

  // if choose to acumulate statistics:
  if (accumulate_stats)
  {
    if (!(std::filesystem::exists(datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/VaryingDValues/" 
                                  + usedAlgorythm + "_TestAccumulativeStatistics.dat")))
    {
      // If the file doesn't exist, create it and write initial statistics
      system (("mkdir -p " + datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/VaryingDValues/").c_str ());
      std::ofstream testAccumulativeStats (datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/VaryingDValues/" 
                                            + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      if (adjustableAlphas)
      {
        testAccumulativeStats
        << "AdjustableAlphas" << " "
        << tcStats.nTotalDroppedPackets << " " 
        << tcStats.nTotalDroppedPacketsHighPriority << " " 
        << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
        testAccumulativeStats.close ();
      }
      else
      {
        testAccumulativeStats
        << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
        << tcStats.nTotalDroppedPackets << " " 
        << tcStats.nTotalDroppedPacketsHighPriority << " " 
        << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
        testAccumulativeStats.close ();
      }
    }
    else
    {
      // Open the file in append mode
      std::fstream testAccumulativeStats (datDir + "/TestStats/" + traffic_control_type + "/" + implementation + "/" + onoff_traffic_mode + "/VaryingDValues/" 
                                          + usedAlgorythm + "_TestAccumulativeStatistics.dat", std::ios::app);
      if (adjustableAlphas)
      {
        testAccumulativeStats
        << "AdjustableAlphas" << " "
        << tcStats.nTotalDroppedPackets << " " 
        << tcStats.nTotalDroppedPacketsHighPriority << " " 
        << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
        testAccumulativeStats.close ();
      }
      else
      {
        testAccumulativeStats
        << DoubleToString(alpha_high) + "_" + DoubleToString(alpha_low) << " "
        << tcStats.nTotalDroppedPackets << " " 
        << tcStats.nTotalDroppedPacketsHighPriority << " " 
        << tcStats.nTotalDroppedPacketsLowPriority << std::endl;
        testAccumulativeStats.close ();
      }
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