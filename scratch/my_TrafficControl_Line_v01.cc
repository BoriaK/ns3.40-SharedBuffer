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
std::string queue_disc_type = "FB_FifoQueueDisc_v01"; // "DT_FifoQueueDisc_v02"/"FB_FifoQueueDisc_v01"

uint32_t prev = 0;
Time prevTime = Seconds (0);

NS_LOG_COMPONENT_DEFINE ("TrafficControlExample_Line_v01_with_CustomApp");

void
TcPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "TcPacketsInQueue " << newValue << std::endl;
}

// Trace the number of High Priority packets in the Queue
void
TcHighPriorityPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream hppiq (dir + queue_disc_type + "/highPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  hppiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  hppiq.close ();

  std::cout << "HighPriorityPacketsInQueue " << newValue << std::endl;
}

// Trace the number of Low Priority packets in the Queue
void
TcLowPriorityPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream lppiq (dir + queue_disc_type + "/lowPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  lppiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  lppiq.close ();

  std::cout << "LowPriorityPacketsInQueue " << newValue << std::endl;
}

// Trace the Threshold Value for High Priority packets in the Queue
void
QueueThresholdHighTrace (float_t oldValue, float_t newValue)  // added by me, to monitor Threshold
{
  std::ofstream hpthr (dir + queue_disc_type + "/highPriorityQueueThreshold.dat", std::ios::out | std::ios::app);
  hpthr << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  hpthr.close ();

  std::cout << "HighPriorityQueueThreshold " << newValue << " packets " << std::endl;
}

// Trace the Threshold Value for Low Priority packets in the Queue
void
QueueThresholdLowTrace (float_t oldValue, float_t newValue)  // added by me, to monitor Threshold
{
  std::ofstream lpthr (dir + queue_disc_type + "/lowPriorityQueueThreshold.dat", std::ios::out | std::ios::app);
  lpthr << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  lpthr.close ();
  
  std::cout << "LowPriorityQueueThreshold " << newValue << " packets " << std::endl;
}

// void
// DropBeforeEnqueueTrace ()  // added by me, to monitor Threshold
// {
//   // std::ofstream pdbe (dir + queue_disc_type + "/packetsDroppedBeforeEnqueueing.dat", std::ios::out | std::ios::app);
//   // pdbe << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
//   // pdbe.close ();
  
//   std::cout << "PacketsDroppedBeforeEnqueueing packets " << std::endl;
// }

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
  // Set up some default values for the simulation.
  double simulationTime = 50; //seconds
  std::string applicationType = "customOnOff"; // "standardClient"/"OnOff"/"customApplication"/"customOnOff"
  std::string transportProt = "Udp";
  std::string socketType;
  std::string queue_capacity;
  uint32_t alpha_high = 2;
  uint32_t alpha_low = 1;
  bool enablePcap = false;  // true/false
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
  if (applicationType.compare("standardClient") == 0)
    {
      queue_capacity = "20p"; // B, the total space on the buffer
    }
  else if (applicationType.compare("OnOff") == 0 || applicationType.compare("customOnOff") == 0 || applicationType.compare("customApplication") == 0)
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


  TrafficControlHelper tch;
  // tch.SetRootQueueDisc ("ns3::RedQueueDisc", "MaxSize", StringValue ("5p"));
  // tch.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue ("20p"));
  tch.SetRootQueueDisc ("ns3::" + queue_disc_type, "MaxSize", StringValue (queue_capacity), 
                        "Alpha_High", DoubleValue (alpha_high), "Alpha_Low", DoubleValue (alpha_low));
                                                   
  QueueDiscContainer qdiscs = tch.Install (dev1);

  // Ptr<QueueDisc> q = qdiscs.Get (1); // original code - doesn't show values
  Ptr<QueueDisc> q = qdiscs.Get (0); // look at the router queue - shows actual values
  // The Next Line Displayes "PacketsInQueue" statistic at the Traffic Controll Layer
  // q->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&TcPacketsInQueueTrace));
  q->TraceConnectWithoutContext ("HighPriorityPacketsInQueue", MakeCallback (&TcHighPriorityPacketsInQueueTrace));  // ### ADDED BY ME #####
  q->TraceConnectWithoutContext ("LowPriorityPacketsInQueue", MakeCallback (&TcLowPriorityPacketsInQueueTrace));  // ### ADDED BY ME #####
  q->TraceConnectWithoutContext("EnqueueingThreshold_High", MakeCallback (&QueueThresholdHighTrace)); // ### ADDED BY ME #####
  q->TraceConnectWithoutContext("EnqueueingThreshold_Low", MakeCallback (&QueueThresholdLowTrace)); // ### ADDED BY ME #####
  // q->TraceConnectWithoutContext("DropBeforeEnqueue", MakeCallback (&DropBeforeEnqueueTrace)); // ### ADDED BY ME #####
  Config::ConnectWithoutContextFailSafe ("/NodeList/1/$ns3::TrafficControlLayer/RootQueueDiscList/0/SojournTime",
                                 MakeCallback (&SojournTimeTrace));                          

  ////////////////////////////////////////////////////////
  // Simulator::ScheduleNow (&CheckQueueSize, q);
  ////////////////////////////////////////////////////////

  // Ptr<NetDevice> nd = dev1.Get (1);  // original value
  Ptr<NetDevice> nd = dev1.Get (0);  //router side? fits queue-discs-benchmark example
  Ptr<PointToPointNetDevice> ptpnd = DynamicCast<PointToPointNetDevice> (nd);
  Ptr<Queue<Packet> > queue = ptpnd->GetQueue ();
  // The Next Line Displayes "PacketsInQueue" statistic at the NetDevice Layer
  // queue->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&DevicePacketsInQueueTrace));


  // Later, we add IP addresses.
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ipv4.Assign (dev0);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ipInterfs = ipv4.Assign (dev1);

  // and setup ip routing tables to get total ip-level connectivity.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t servPort = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), servPort));
  // Create a packet sink to receive these packets on n2
  PacketSinkHelper sink (socketType, sinkLocalAddress);                       
  ApplicationContainer sinkApp = sink.Install (n1n2.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime + 0.1));

  uint32_t payloadSize = 1024;
  uint32_t numOfPackets = 100;  // number of packets to send in one stream for custom application

  if (applicationType.compare("standardClient") == 0)
  {
    // Install UDP application on the sender  
    UdpClientHelper clientHelper (ipInterfs.GetAddress(1), servPort);
    clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    ApplicationContainer sourceApps = clientHelper.Install (n0n1.Get (0));
    sourceApps.Start (Seconds (1.0));
    sourceApps.Stop (Seconds(3.0));
  }
  
  else if (applicationType.compare("OnOff") == 0)
  {
    // Create the OnOff applications to send TCP/UDP to the server
    InetSocketAddress socketAddressUp = InetSocketAddress (ipInterfs.GetAddress(1), servPort);
    OnOffHelper clientHelper (socketType, Address ());
    clientHelper.SetAttribute ("Remote", AddressValue (socketAddressUp));
    clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
    clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    clientHelper.SetAttribute ("DataRate", StringValue ("2Mb/s"));
    ApplicationContainer sourceApps = clientHelper.Install (n0n1.Get (0));
    sourceApps.Start (Seconds (1.0));
    sourceApps.Stop (Seconds(3.0));
  }
  
  else if (applicationType.compare("customOnOff") == 0)
  {
    // Create the Custom application to send TCP/UDP to the server
    Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (n0n1.Get (0), UdpSocketFactory::GetTypeId ());
    // ns3UdpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
    
    InetSocketAddress socketAddressUp = InetSocketAddress (ipInterfs.GetAddress(1), servPort);
    Ptr<CustomOnOffApplication> customOnOffApp = CreateObject<CustomOnOffApplication> ();
    customOnOffApp->Setup(ns3UdpSocket);
    customOnOffApp->SetAttribute("Remote", AddressValue (socketAddressUp));
    customOnOffApp->SetAttribute("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
    customOnOffApp->SetAttribute("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
    customOnOffApp->SetAttribute("PacketSize", UintegerValue (payloadSize));
    customOnOffApp->SetAttribute("DataRate", StringValue ("2Mb/s"));
    customOnOffApp->SetAttribute("EnableSeqTsSizeHeader", BooleanValue (false));
    customOnOffApp->SetStartTime (Seconds (1.0));
    /////////////////////
    // Hook trace source after application starts
    // Simulator::Schedule (Seconds (1.0) + MilliSeconds (1), &TraceCwnd, 0, 0);
    ////////////////////////////
    customOnOffApp->SetStopTime (Seconds(3.0));
    n0n1.Get (0)->AddApplication (customOnOffApp);
  }
  
  else if (applicationType.compare("customApplication") == 0)
  {
    // Create the Custom application to send TCP/UDP to the server
    Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (n0n1.Get (0), UdpSocketFactory::GetTypeId ());
    // ns3UdpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
    
    Ptr<TutorialApp> customApp = CreateObject<TutorialApp> ();
    InetSocketAddress socketAddressUp = InetSocketAddress (ipInterfs.GetAddress(1), servPort);  // sink IpV4 Address
    customApp->Setup (ns3UdpSocket, socketAddressUp, payloadSize, numOfPackets, DataRate ("1Mbps"));
    customApp->SetStartTime (Seconds (1.0));
    /////////////////////
    // Hook trace source after application starts
    // Simulator::Schedule (Seconds (1.0) + MilliSeconds (1), &TraceCwnd, 0, 0);
    ////////////////////////////
    customApp->SetStopTime (Seconds(3.0));
    n0n1.Get (0)->AddApplication (customApp);
  }
  
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  std::string dirToSave = "mkdir -p " + dir + queue_disc_type;
  if (system (dirToSave.c_str ()) == -1)
    {
      exit (1);
    }  
  
  // Generate PCAP traces if it is enabled
  if (enablePcap)
    {
      // NS_LOG_INFO ("Enable pcap tracing.");
      if (system ((dirToSave + "/pcap/").c_str ()) == -1)
        {
          exit (1);
        }
      p2p2.EnablePcapAll (dir + queue_disc_type + "/pcap/dt", true);
    }

  Simulator::Stop (Seconds (simulationTime + 10));
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl << "Topology: Line" << std::endl;
  std::cout << std::endl << "Queueing Algorithm: " + queue_disc_type << std::endl;
  std::cout << std::endl << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
  std::cout << std::endl << "Application: " + applicationType << std::endl;

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

  // Simulator::Destroy ();
   
  std::cout << std::endl << "*** Application statistics ***" << std::endl;
  double thr = 0;
  uint64_t totalPacketsThr = DynamicCast<PacketSink> (sinkApp.Get (0))->GetTotalRx ();
  thr = totalPacketsThr * 8 / (simulationTime * 1000000.0); //Mbit/s
  std::cout << "  Rx Bytes: " << totalPacketsThr << std::endl;
  std::cout << "  Average Goodput: " << thr << " Mbit/s" << std::endl;
  std::cout << std::endl << "*** TC Layer statistics ***" << std::endl;
  std::cout << q->GetStats () << std::endl;
  
  // Added to create a .txt file with the summary of the tested scenario statistics
  std::ofstream testFlowStatistics (dir + queue_disc_type + "/Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: Line" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + queue_disc_type << std::endl;
  testFlowStatistics << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
  testFlowStatistics << "Application: " + applicationType << std::endl; 
  testFlowStatistics << std::endl << "*** Flow monitor statistics ***" << std::endl;
  testFlowStatistics << "  Tx Packets/Bytes:   " << stats[1].txPackets
                     << " / " << stats[1].txBytes << std::endl;
  testFlowStatistics << "  Rx Packets/Bytes:   " << stats[1].rxPackets
                     << " / " << stats[1].rxBytes << std::endl;
  testFlowStatistics << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc
            << " / " << bytesDroppedByQueueDisc << std::endl;
  testFlowStatistics << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice
            << " / " << bytesDroppedByNetDevice << std::endl;
  testFlowStatistics << "  Throughput: " << stats[1].rxBytes * 8.0 / (stats[1].timeLastRxPacket.GetSeconds () - stats[1].timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;                   
  testFlowStatistics << std::endl << "*** TC Layer statistics ***" << std::endl;
  testFlowStatistics << q->GetStats () << std::endl;
  testFlowStatistics.close ();

  // command line needs to be in ./scratch/ inorder for the script to produce gnuplot correctly///
  // system (("gnuplot " + dir + "gnuplotScriptTcHighPriorityPacketsInQueue").c_str ());

  Simulator::Destroy ();
  return 0;
}