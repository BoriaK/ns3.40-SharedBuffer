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

// This was an attempt to build a Tree Topology by connecting the hub of a star topology
// to an additional node as a reciever.
//
// Network topology (default)
//       
//       n0                       .
//         \                      .
//          \                     .
//      n1---R------------S       .          
//       .  /                     .
//       . /                      .
//       nN                       .
//       
//
//  Usage (e.g.): ./ns3 run scratch/my_lineTopology_v02

#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Traffic_Control_Example_Incast_Topology_v02");

void
TcPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "TcPacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

void
DevicePacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "DevicePacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

void
SojournTimeTrace (Time sojournTime)
{
  std::cout << "Sojourn time " << sojournTime.ToDouble (Time::MS) << "ms" << std::endl;
}

int main (int argc, char *argv[])
{
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
  //  LogComponentEnable("TcpL4Protocol", LOG_LEVEL_ALL);
  //  LogComponentEnable("TcpSocketImpl", LOG_LEVEL_ALL);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
  //  LogComponentEnable("TcpLargeTransfer", LOG_LEVEL_ALL);

  // Here, we will explicitly create all the nodes.  The first container contains
  // the star diagram above, and the second one contains nodes:
  // star hub and the reciever.  This reflects the channel connectivity, and will be used to
  // install the network interfaces and connect them with a channel.

  uint32_t N = 3; //number of sender nodes in the tree. Overridable by command line argument.
  double simulationTime = 40; //seconds
  std::string transportProt = "Udp";
  std::string socketType;
  
  CommandLine cmd (__FILE__);
  cmd.AddValue ("nSpokes", "Number of nodes to place in the tree", N);
  cmd.AddValue("Simulation Time", "The total time for the simulation to run",simulationTime);
  cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, Udp", transportProt);
  cmd.Parse (argc, argv);

  // build star, all spokes are sender nodes. the hub is a router node.
  // create the link device that will connect all the senders to the router
  // connect spokes to the hub and install the desired device (channel).
  
  NS_LOG_INFO ("Build star topology.");
  // Here, we will create N nodes in a star.
  NS_LOG_INFO ("Create nodes.");
  NodeContainer serverNodes;
  NodeContainer clientNodes;
  serverNodes.Create (2);
  clientNodes.Create (N);
  NodeContainer allNodes = NodeContainer (serverNodes, clientNodes);

  // Install network stacks on the nodes
  InternetStackHelper internet;
  internet.Install (allNodes);

  //Collect an adjacency list of nodes for the p2p topology
  std::vector<NodeContainer> nodeAdjacencyList (N);
  for(uint32_t i=0; i<nodeAdjacencyList.size (); ++i)
    {
      nodeAdjacencyList[i] = NodeContainer (serverNodes.Get(0), clientNodes.Get (i));
    }

  // We create the channels first without any IP addressing information
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p1; // the link between senders and router
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("2ms"));
  std::vector<NetDeviceContainer> deviceAdjacencyList (N);
  for(uint32_t i=0; i<deviceAdjacencyList.size (); ++i)
    {
      deviceAdjacencyList[i] = p2p1.Install (nodeAdjacencyList[i]);
    }
  
  PointToPointHelper p2p2;  // the link between router and Reciever
  p2p2.SetDeviceAttribute  ("DataRate", StringValue ("5Kbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("10ms"));
  // min value for NetDevice buffer is 1p. we set it in order to observe Traffic Controll effects only.
  p2p2.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));

  // then install devices and channels connecting our router to reciever.
  NetDeviceContainer serverDevice = p2p2.Install (serverNodes);


  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::RedQueueDisc", "MaxSize", StringValue ("10p"));
  QueueDiscContainer qdiscs = tch.Install (serverDevice);

  // Ptr<QueueDisc> q = qdiscs.Get (1); // original code - doesn't show values
  Ptr<QueueDisc> q = qdiscs.Get (0); // look at the router queue - shows actual values
  // The Next Line Displayes "PacketsInQueue" statistic at the Traffic Controll Layer
  // q->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&TcPacketsInQueueTrace));
  Config::ConnectWithoutContextFailSafe ("/NodeList/1/$ns3::TrafficControlLayer/RootQueueDiscList/0/SojournTime",
                                 MakeCallback (&SojournTimeTrace));

  // Ptr<NetDevice> nd = serverDevice.Get (1);  // original value
  Ptr<NetDevice> nd = serverDevice.Get (0);  //router side? fits queue-discs-benchmark example
  Ptr<PointToPointNetDevice> ptpnd = DynamicCast<PointToPointNetDevice> (nd);
  Ptr<Queue<Packet> > queue = ptpnd->GetQueue ();
  // The Next Line Displayes "PacketsInQueue" statistic at the NetDevice Layer
  // queue->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&DevicePacketsInQueueTrace));


  // Assign IP addresses
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  // ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  std::vector<Ipv4InterfaceContainer> interfaceAdjacencyList (N);
  for(uint32_t i=0; i<interfaceAdjacencyList.size (); ++i)
    {
      std::ostringstream subnet;
      subnet<<"10.1."<<i+1<<".0";
      ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
      interfaceAdjacencyList[i] = ipv4.Assign (deviceAdjacencyList[i]);
    }
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterface = ipv4.Assign (serverDevice);

  // and setup ip routing tables to get total ip-level connectivity.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create applications.");
  // Create a packet sink to collect packets at the Reciever
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (serverNodes.Get(1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime));

  // Install application on the senders
  //get the address of the reciever node NOT THE ROUTER!!!
  // // Create the OnOff applications to send TCP to the server
  // OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address ());
  // clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  // clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  
  // FOR DEBUG: TRY TO SEND udp PACKETS FROM CLIENT TO SERVER WITHOUT OnOffApplication
  UdpClientHelper clientHelper (routerInterface.GetAddress(1), port);
  clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  //normally wouldn't need a loop here but the server IP address is different
  //on each p2p subnet
  ApplicationContainer clientApps;
  for(uint32_t i=0; i<clientNodes.GetN (); ++i)
    {
      clientApps.Add (clientHelper.Install (clientNodes.Get (i)));
    }
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (2.0));

  NS_LOG_INFO ("Run Simulation.");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (simulationTime + 10));
  Simulator::Run ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
  std::cout << "  Tx Packets/Bytes:   " << stats[1].txPackets
            << " / " << stats[1].txBytes << std::endl;
  std::cout << "  Offered Load: " << stats[1].txBytes * 8.0 / (stats[1].timeLastTxPacket.GetSeconds () - stats[1].timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
  std::cout << "  Rx Packets/Bytes:   " << stats[1].rxPackets
            << " / " << stats[1].rxBytes << std::endl;
  uint32_t packetsDroppedByQueueDisc = 0;
  uint64_t bytesDroppedByQueueDisc = 0;
  if (stats[1].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE_DISC)
    {
      packetsDroppedByQueueDisc = stats[1].packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
      bytesDroppedByQueueDisc = stats[1].bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
    }
  std::cout << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc
            << " / " << bytesDroppedByQueueDisc << std::endl;
  uint32_t packetsDroppedByNetDevice = 0;
  uint64_t bytesDroppedByNetDevice = 0;
  if (stats[1].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE)
    {
      packetsDroppedByNetDevice = stats[1].packetsDropped[Ipv4FlowProbe::DROP_QUEUE];
      bytesDroppedByNetDevice = stats[1].bytesDropped[Ipv4FlowProbe::DROP_QUEUE];
    }
  std::cout << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice
            << " / " << bytesDroppedByNetDevice << std::endl;
  std::cout << "  Throughput: " << stats[1].rxBytes * 8.0 / (stats[1].timeLastRxPacket.GetSeconds () - stats[1].timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
  std::cout << "  Mean delay:   " << stats[1].delaySum.GetSeconds () / stats[1].rxPackets << std::endl;
  std::cout << "  Mean jitter:   " << stats[1].jitterSum.GetSeconds () / (stats[1].rxPackets - 1) << std::endl;
  auto dscpVec = classifier->GetDscpCounts (1);
  for (auto p : dscpVec)
    {
      std::cout << "  DSCP value:   0x" << std::hex << static_cast<uint32_t> (p.first) << std::dec
                << "  count:   "<< p.second << std::endl;
    }

  Simulator::Destroy ();

  std::cout << std::endl << "*** Application statistics ***" << std::endl;
  double thr = 0;
  uint64_t totalPacketsThr = DynamicCast<PacketSink> (sinkApp.Get (0))->GetTotalRx ();
  thr = totalPacketsThr * 8 / (simulationTime * 1000000.0); //Mbit/s
  std::cout << "  Rx Bytes: " << totalPacketsThr << std::endl;
  std::cout << "  Average Goodput: " << thr << " Mbit/s" << std::endl;
  std::cout << std::endl << "*** TC Layer statistics ***" << std::endl;
  std::cout << q->GetStats () << std::endl;
  return 0;
}