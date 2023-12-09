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

//
// Network topology
//
//           10Mb/s, 10ms       100Kb/s, 10ms
//       n0-----------------n1-----------------n2
//
//
// - Tracing of queues and packet receptions to file 
//   "tcp-large-transfer.tr"
// - pcap traces also generated in the following files
//   "tcp-large-transfer-$n-$i.pcap" where n and i represent node and interface
// numbers respectively
//  Usage (e.g.): ./ns3 run scratch/my_lineTopology_v01

#include <iostream>
#include <fstream>
#include <string>

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


using namespace ns3;

std::string dir = "./Trace_Plots/Line_Topology/";
std::string queue_disc_type = "PrioQueueDisc"; // "PrioQueueDisc"

uint32_t prev = 0;
Time prevTime = Seconds (0);

NS_LOG_COMPONENT_DEFINE ("Line_v01_with_CustomApp_SharedBuffer");

void
QueueDiscPacketsInQueueTrace (size_t queueIndex, uint32_t oldValue, uint32_t newValue)
{
  std::cout << "QueueDiscPacketsInQueue " << queueIndex << ": " << newValue << std::endl;
}

void
HighPriorityQueueDiscPacketsInQueueTrace (size_t queueIndex, uint32_t oldValue, uint32_t newValue)
{
  std::cout << "HighPriorityQueueDiscPacketsInQueue " << queueIndex << ": " << newValue << std::endl;
}

void
LowPriorityQueueDiscPacketsInQueueTrace (size_t queueIndex, uint32_t oldValue, uint32_t newValue)
{
  std::cout << "LowPriorityQueueDiscPacketsInQueue " << queueIndex << ": " << newValue << std::endl;
}

// Trace the Threshold Value for High Priority packets in the Queue
// void
// QueueThresholdHighTrace (float_t oldValue, float_t newValue)  // added by me, to monitor Threshold
// {
//   std::ofstream hpthr (dir + queue_disc_type + "/highPriorityQueueThreshold.dat", std::ios::out | std::ios::app);
//   hpthr << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
//   hpthr.close ();

//   std::cout << "HighPriorityQueueThreshold " << newValue << " packets " << std::endl;
// }

// Trace the Threshold Value for Low Priority packets in the Queue
// void
// QueueThresholdLowTrace (float_t oldValue, float_t newValue)  // added by me, to monitor Threshold
// {
//   std::ofstream lpthr (dir + queue_disc_type + "/lowPriorityQueueThreshold.dat", std::ios::out | std::ios::app);
//   lpthr << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
//   lpthr.close ();
  
//   std::cout << "LowPriorityQueueThreshold " << newValue << " packets " << std::endl;
// }

void
DevicePacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "DevicePacketsInQueue " << newValue << std::endl;
}

void
SojournTimeTrace (Time sojournTime)
{
  std::cout << "Sojourn time " << sojournTime.ToDouble (Time::MS) << "ms" << std::endl;
}

int main (int argc, char *argv[])
{ 
  // Set up some default values for the simulation.
  double simulationTime = 50; //seconds
  std::string applicationType = "PrioOnOff"; // "standardClient"/"OnOff"/"prioClient"/"PrioOnOff"
  std::string transportProt = "Udp";
  std::string socketType;
  std::string queue_capacity;
  bool eraseOldData = true; // true/false


  if (eraseOldData == true)
  {
    system (("rm " + dir + queue_disc_type + "/*.dat").c_str ()); // to erasethe old .dat files and collect new data
    system (("rm " + dir + queue_disc_type + "/*.txt").c_str ()); // to erasethe previous test run summary, and collect new data
    std::cout << std::endl << "***Erased Previous Data***\n" << std::endl;
  }
  

  CommandLine cmd (__FILE__);
  cmd.AddValue("Simulation Time", "The total time for the simulation to run", simulationTime);
  cmd.AddValue ("applicationType", "Application type to use to send data: OnOff, standardClient, customOnOff", applicationType);
  cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, Udp", transportProt);
  cmd.Parse (argc, argv);
  
  // Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  // Config::SetDefault ("ns3::UdpSocket::InitialCwnd", UintegerValue (1));
  // Config::SetDefault ("ns3::TcpL4Protocol::RecoveryType", TypeIdValue (TypeId::LookupByName ("ns3::TcpClassicRecovery")));

  // Application type dependent parameters
  if (applicationType.compare("standardClient") == 0 || applicationType.compare("prioClient") == 0)
    {
      queue_capacity = "10p"; // B, the total space on the buffer
    }
  else if (applicationType.compare("OnOff") == 0 || applicationType.compare("PrioOnOff") == 0)
    {
      queue_capacity = "100p"; // B, the total space on the buffer [packets]
    }
  // client type dependant parameters:
  if (transportProt.compare ("Tcp") == 0)
    {
      socketType = "ns3::TcpSocketFactory";
    }
  else
    {
      socketType = "ns3::UdpSocketFactory";
    }
  
  // Application and Client type dependent parameters
  // select the desired components to output data
  if (applicationType.compare("standardClient") == 0 && transportProt.compare ("Tcp") == 0)
  {
    LogComponentEnable ("TcpClient", LOG_LEVEL_INFO);
  }
  else if (applicationType.compare("standardClient") == 0 && transportProt.compare ("Udp") == 0)
  {
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  }
  else if ((applicationType.compare("OnOff") == 0 || applicationType.compare("customOnOff") == 0 || applicationType.compare("customApplication") == 0)&& transportProt.compare ("Tcp") == 0)
  {
    LogComponentEnable("TcpSocketImpl", LOG_LEVEL_INFO);
  }
  else if ((applicationType.compare("OnOff") == 0 || applicationType.compare("customOnOff") == 0 || applicationType.compare("customApplication") == 0) && transportProt.compare ("Udp") == 0)
  {
    LogComponentEnable("UdpSocketImpl", LOG_LEVEL_INFO);
  }
  
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO); 
  

  // Here, we will explicitly create three nodes.  The first container contains
  // nodes 0 and 1 from the diagram above, and the second one contains nodes
  // 1 and 2.  This reflects the channel connectivity, and will be used to
  // install the network interfaces and connect them with a channel.
  NodeContainer n0n1;
  n0n1.Create (2);

  NodeContainer n1n2;
  n1n2.Add (n0n1.Get (1));
  n1n2.Create (1);

  // We create the channels first without any IP addressing information
  // First make and configure the helper, so that it will put the appropriate
  // attributes on the network interfaces and channels we are about to install.
  
  // Create the point-to-point link helpers
  PointToPointHelper p2p1;  // the link from sender to Router
  p2p1.SetDeviceAttribute  ("DataRate", StringValue ("10Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("5ms"));
  p2p1.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));

  PointToPointHelper p2p2;  // the link between router and Reciever
  p2p2.SetDeviceAttribute  ("DataRate", StringValue ("100Kbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("10ms"));
  // min value for NetDevice buffer is 1p. we set it in order to observe Traffic Controll effects only.
  p2p2.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));

  // And then install devices and channels connecting our topology.
  NetDeviceContainer dev0 = p2p1.Install (n0n1);
  NetDeviceContainer dev1 = p2p2.Install (n1n2);

  // Now add ip/tcp stack to all nodes.
  InternetStackHelper internet;
  internet.InstallAll ();


  NS_LOG_INFO ("Install QueueDisc");

  // Create a TrafficControlHelper object:
  TrafficControlHelper tch;
  // uint16_t handle = tch.SetRootQueueDisc("ns3::PrioQueueDisc", "Priomap", StringValue("0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1"));

  // priomap with low priority for class "0" and high priority for rest of the 15 classes (1-15) ?
    uint16_t handle = tch.SetRootQueueDisc("ns3::RoundRobinPrioQueueDisc", "Priomap", StringValue("1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0")); 

  TrafficControlHelper::ClassIdList cid = tch.AddQueueDiscClasses(handle, 2, "ns3::QueueDiscClass");

  tch.AddChildQueueDisc(handle, cid[0], "ns3::FifoQueueDisc" , "MaxSize", StringValue (queue_capacity)); // cid[0] is band "0" - the Highest Priority band
  tch.AddChildQueueDisc(handle, cid[1], "ns3::FifoQueueDisc", "MaxSize", StringValue (queue_capacity)); // cid[1] is Low Priority

  
  QueueDiscContainer qdisc = tch.Install(dev1.Get(0));

/////////////////////////////Monitor the NetDevice/////////////////////////////////////
  Ptr<NetDevice> nd = dev1.Get (0);
  Ptr<PointToPointNetDevice> ptpnd = DynamicCast<PointToPointNetDevice> (nd);
  Ptr<Queue<Packet> > queue = ptpnd->GetQueue ();
  // The Next Line Displayes "PacketsInQueue" statistic at the NetDevice Layer
  queue->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&DevicePacketsInQueueTrace));
/////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////extract data from q-disc//////////////////////////////
  for (size_t i = 0; i < qdisc.Get(0)->GetNQueueDiscClasses(); i++)
  {
      qdisc.Get(0)->GetQueueDiscClass(i)->GetQueueDisc()->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&QueueDiscPacketsInQueueTrace, i));
      qdisc.Get(0)->GetQueueDiscClass(i)->GetQueueDisc()->TraceConnectWithoutContext ("HighPriorityPacketsInQueue", MakeBoundCallback (&HighPriorityQueueDiscPacketsInQueueTrace, i));
      qdisc.Get(0)->GetQueueDiscClass(i)->GetQueueDisc()->TraceConnectWithoutContext ("LowPriorityPacketsInQueue", MakeBoundCallback (&LowPriorityQueueDiscPacketsInQueueTrace, i));
  }

/////////////////////////////////////////////////////////////////////////////////

  // Later, we add IP addresses.
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ipv4.Assign (dev0);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ipInterfs = ipv4.Assign (dev1);

  // and setup ip routing tables to get total ip-level connectivity.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t servPortP0 = 50000;
  uint16_t servPortP1 = 50001;
  
  Address sinkLocalAddressP0 (InetSocketAddress (Ipv4Address::GetAny (), servPortP0));
  Address sinkLocalAddressP1 (InetSocketAddress (Ipv4Address::GetAny (), servPortP1));

  // Create packet sinks to receive packets with distinct priorities on n2
  PacketSinkHelper sinkP0 (socketType, sinkLocalAddressP0);
  PacketSinkHelper sinkP1 (socketType, sinkLocalAddressP1);                       
  
  ApplicationContainer sourceApps, sinkApps;
  sinkApps.Add(sinkP0.Install (n1n2.Get (1)));
  sinkApps.Add(sinkP1.Install (n1n2.Get (1)));

  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime + 0.1));

  uint32_t payloadSize = 1024;
  // uint32_t numOfPackets = 100;  // number of packets to send in one stream for custom application
  uint32_t ipTos_LP = 0x00; //Low priority: Best Effort
	uint32_t ipTos_HP = 0x10;  //High priority: Maximize Throughput

  // Install UDP application on the sender
  InetSocketAddress socketAddressP0 = InetSocketAddress (ipInterfs.GetAddress(1), servPortP0);
  socketAddressP0.SetTos(ipTos_HP);  // 0x10 -> High priority

  InetSocketAddress socketAddressP1 = InetSocketAddress (ipInterfs.GetAddress(1), servPortP1);
    socketAddressP1.SetTos(ipTos_LP);  // 0x0 -> Low priority

  if (applicationType.compare("standardClient") == 0)
  {
    UdpClientHelper clientHelperP0 (socketAddressP0);
    clientHelperP0.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    clientHelperP0.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    
    UdpClientHelper clientHelperP1 (socketAddressP1);
    clientHelperP1.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    clientHelperP1.SetAttribute ("PacketSize", UintegerValue (payloadSize));

    sourceApps.Add(clientHelperP0.Install (n0n1.Get (0)));
    sourceApps.Add(clientHelperP1.Install (n0n1.Get (0)));

    sourceApps.Start (Seconds (1.0));
    sourceApps.Stop (Seconds(3.0));
  }
  else if (applicationType.compare("OnOff") == 0)
  {
    // Create the OnOff applications to send TCP/UDP to the server
    OnOffHelper clientHelperP0 (socketType, socketAddressP0);
    clientHelperP0.SetAttribute ("Remote", AddressValue (socketAddressP0));
    clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
    clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
    clientHelperP0.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    clientHelperP0.SetAttribute ("DataRate", StringValue ("2Mb/s"));
    
    OnOffHelper clientHelperP1 (socketType, socketAddressP1);
    clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
    clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
    clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
    clientHelperP1.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    clientHelperP1.SetAttribute ("DataRate", StringValue ("2Mb/s"));
    
    sourceApps.Add(clientHelperP0.Install (n0n1.Get (0)));
    sourceApps.Add(clientHelperP1.Install (n0n1.Get (0)));
    
    sourceApps.Start (Seconds (1.0));
    sourceApps.Stop (Seconds(3.0));
  }
  else if (applicationType.compare("prioClient") == 0)
  {
    UdpPrioClientHelper clientHelperP0 (socketAddressP0);
    clientHelperP0.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    clientHelperP0.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    // clientHelperP0.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
    clientHelperP0.SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
    
    UdpPrioClientHelper clientHelperP1 (socketAddressP1);
    clientHelperP1.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    clientHelperP1.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    // clientHelperP1.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
    clientHelperP1.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low

    sourceApps.Add(clientHelperP0.Install (n0n1.Get (0)));
    sourceApps.Add(clientHelperP1.Install (n0n1.Get (0)));

    sourceApps.Start (Seconds (1.0));
    sourceApps.Stop (Seconds(3.0));
  }
  else if (applicationType.compare("PrioOnOff") == 0)
  {
    // Create the OnOff applications to send TCP/UDP to the server
    PrioOnOffHelper clientHelperP0 (socketType, socketAddressP0);
    clientHelperP0.SetAttribute ("Remote", AddressValue (socketAddressP0));
    clientHelperP0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
    clientHelperP0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
    clientHelperP0.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    clientHelperP0.SetAttribute ("DataRate", StringValue ("2Mb/s"));
    // clientHelperP0.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
    clientHelperP0.SetAttribute("FlowPriority", UintegerValue (0x1));  // manualy set generated packets priority: 0x1 high, 0x2 low
    
    PrioOnOffHelper clientHelperP1 (socketType, socketAddressP1);
    clientHelperP1.SetAttribute ("Remote", AddressValue (socketAddressP1));
    clientHelperP1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
    clientHelperP1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
    clientHelperP1.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    clientHelperP1.SetAttribute ("DataRate", StringValue ("2Mb/s"));
    // clientHelperP1.SetAttribute("NumOfPacketsHighPrioThreshold", UintegerValue (10)); // relevant only if "FlowPriority" NOT set by user
    clientHelperP1.SetAttribute("FlowPriority", UintegerValue (0x2));  // manualy set generated packets priority: 0x1 high, 0x2 low
    
    sourceApps.Add(clientHelperP0.Install (n0n1.Get (0)));
    sourceApps.Add(clientHelperP1.Install (n0n1.Get (0)));
  }
  
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds(3.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  std::string dirToSave = "mkdir -p " + dir + queue_disc_type;
  if (system (dirToSave.c_str ()) == -1)
    {
      exit (1);
    }  
  
  NS_LOG_INFO ("Start simulation");
    Simulator::Stop (Seconds (simulationTime + 10));
    Simulator::Run ();

    // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
    std::cout << std::endl << "Topology: Line" << std::endl;
    std::cout << std::endl << "Queueing Algorithm: " + queue_disc_type << std::endl;
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

    goodTpT = totalBytesRx * 8 / (simulationTime * 1000000.0); //Mbit/s
    std::cout << "  Rx Bytes: " << totalBytesRx << std::endl;
    std::cout << "  Average Goodput: " << goodTpT << " Mbit/s" << std::endl;

    std::cout << std::endl << "*** TC Layer statistics ***" << std::endl;
    for (size_t i = 0; i < qdisc.GetN(); i++)
    {
        std::cout << "Queue Disceplene " << i << ":" << std::endl;
        std::cout << qdisc.Get(i)->GetStats () << std::endl;
    }

    
    // Added to create a .txt file with the summary of the tested scenario statistics
    std::ofstream testFlowStatistics (dir + queue_disc_type + "/Statistics.txt", std::ios::out | std::ios::app);
    testFlowStatistics << "Topology: 2In2Out" << std::endl;
    testFlowStatistics << "Queueing Algorithm: " + queue_disc_type << std::endl;
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
    for (size_t i = 0; i < qdisc.GetN(); i++)
    {
        testFlowStatistics << "Queue Disceplene " << i << ":" << std::endl;
        testFlowStatistics << qdisc.Get(i)->GetStats () << std::endl;
    }
    testFlowStatistics.close ();

  // command line needs to be in ./scratch/ inorder for the script to produce gnuplot correctly///
  // system (("gnuplot " + dir + "gnuplotScriptTcHighPriorityPacketsInQueue").c_str ());

  Simulator::Destroy ();
  return 0;
}