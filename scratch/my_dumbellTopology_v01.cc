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
 *     L1               R1
 *      \              /
 *       \+--+    +--+/
 *   L2---|LR|----|RR|---R2
 *       /+--+    +--|\
 *      /              \
 *    L3                R3
*/

//  Usage (e.g.): ./ns3 run scratch/my_dumbellTopology_v01

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

std::string dir = "./Trace_Plots/Dumbell_Topology/";
std::string queue_disc_type = "RedQueueDisc"; // "RedQueueDisc"/**"MqQueueDisc"**/"DT_FifoQueueDisc_v02"/"FB_FifoQueueDisc_v01"

uint32_t prev = 0;
Time prevTime = Seconds (0);

NS_LOG_COMPONENT_DEFINE ("TrafficControlExample_Dumbell_v01_with_CustomApp");


void
TcPacketsInQueue0_Trace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "TcPacketsInQueue0: " << newValue << std::endl;
}

void
TcPacketsInQueue1_Trace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "TcPacketsInQueue1: " << newValue << std::endl;
}

void
TcPacketsInQueue2_Trace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "TcPacketsInQueue2: " << newValue << std::endl;
}

// Trace the number of High Priority packets in the Queue
// void
// TcHighPriorityPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
// {
//   std::ofstream hppiq (dir + queue_disc_type + "/highPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
//   hppiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
//   hppiq.close ();

//   std::cout << "HighPriorityPacketsInQueue " << newValue << std::endl;
// }

// Trace the number of Low Priority packets in the Queue
// void
// TcLowPriorityPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
// {
//   std::ofstream lppiq (dir + queue_disc_type + "/lowPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
//   lppiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
//   lppiq.close ();

//   std::cout << "LowPriorityPacketsInQueue " << newValue << std::endl;
// }

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
  std::string bottleneck_bandwidth = "1Mbps";
  std::string access_bandwidth = "10Mbps";
  std::string bottleneck_delay = "5ms";
  std::string access_delay = "1ms";
  // std::string queuesize = "90p";
  std::string applicationType = "OnOff"; // "standardClient"/"OnOff"/"customApplication"/"customOnOff"
  std::string transportProt = "UDP"; // "UDP"/"TCP"
  std::string socketType;
  std::string queue_capacity;
  // uint32_t alpha_high = 2;
  // uint32_t alpha_low = 1;
  // bool enablePcap = false;  // true/false
  bool eraseOldData = true; // true/false
  uint32_t run = 0;  // not sure what this is for


  if (eraseOldData == true)
  {
    system (("rm " + dir + queue_disc_type + "/*.dat").c_str ()); // to erasethe old .dat files and collect new data
    system (("rm " + dir + queue_disc_type + "/*.txt").c_str ()); // to erasethe previous test run summary, and collect new data
    std::cout << std::endl << "***Erased Previous Data***\n" << std::endl;
  }
  

  CommandLine cmd (__FILE__);
  cmd.AddValue("Simulation Time", "The total time for the simulation to run", simulationTime);
  cmd.AddValue ("applicationType", "Application type to use to send data: OnOff, standardClient, customOnOff", applicationType);
  cmd.AddValue ("transportProt", "Transport protocol to use: TCP, UDP", transportProt);
  cmd.Parse (argc, argv);
  
  // not sure what these 2 lines of code are for
  ns3::SeedManager::SetSeed(1);
  ns3::SeedManager::SetRun(run);

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
  else if ((applicationType.compare("OnOff") == 0 || applicationType.compare("customOnOff") == 0 || applicationType.compare("customApplication") == 0)&& transportProt.compare ("Tcp") == 0)
  {
    LogComponentEnable("TcpSocketImpl", LOG_LEVEL_INFO);
  }
  else if ((applicationType.compare("OnOff") == 0 || applicationType.compare("customOnOff") == 0 || applicationType.compare("customApplication") == 0) && transportProt.compare ("Udp") == 0)
  {
    LogComponentEnable("UdpSocketImpl", LOG_LEVEL_INFO);
  }
  
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO); 

  // We create the channels first without any IP addressing information
  // First make and configure the helper, so that it will put the appropriate
  // attributes on the network interfaces and channels we are about to install.

  uint32_t number_of_clients = 2;  // desided arbitrary # of clients and # of recievers
  std::cerr << "creating " << number_of_clients <<
      " data streams" << std::endl;

  PointToPointHelper bottleNeckLink;
  bottleNeckLink.SetDeviceAttribute("DataRate",
          ns3::StringValue(bottleneck_bandwidth));
  bottleNeckLink.SetChannelAttribute("Delay",
          ns3::StringValue(bottleneck_delay));
  bottleNeckLink.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));

  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute("DataRate",
          ns3::StringValue(access_bandwidth));
  pointToPointLeaf.SetChannelAttribute("Delay",
          ns3::StringValue(access_delay));
  pointToPointLeaf.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));

  /* we cannot use the PointToPointDumbbellHelper because it hides the
    * NetDeviceContainer it creates from us. Therefore, we are creating the
    * dumbbell topology manually */
  // create all the nodes
  NodeContainer routers;
  routers.Create(2);
  NodeContainer leftleaves;
  leftleaves.Create(number_of_clients);
  NodeContainer rightleaves;
  rightleaves.Create(number_of_clients);

  // add the link connecting the routers
  NetDeviceContainer routerdevices =
      bottleNeckLink.Install(routers);

  NetDeviceContainer leftrouterdevices;
  NetDeviceContainer leftleafdevices;
  NetDeviceContainer rightrouterdevices;
  NetDeviceContainer rightleafdevices;

  // add links on both sides
  for (uint32_t i = 0; i<number_of_clients; ++i) 
  {
      // add the left side links
      NetDeviceContainer cleft =
          pointToPointLeaf.Install(routers.Get(0), leftleaves.Get(i));
      leftrouterdevices.Add(cleft.Get(0));
      leftleafdevices.Add(cleft.Get(1));

      // add the right side links
      NetDeviceContainer cright =
          pointToPointLeaf.Install(routers.Get(1), rightleaves.Get(i));
      rightrouterdevices.Add(cright.Get(0));
      rightleafdevices.Add(cright.Get(1));
  }


  // Now add ip/tcp stack to all nodes. this is a VERY IMPORTANT COMPONENT!!!!
  InternetStackHelper internet;
  internet.InstallAll ();


  TrafficControlHelper tch;
  // // tch.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue ("20p"));
  // tch.SetRootQueueDisc ("ns3::" + queue_disc_type, "MaxSize", StringValue (queue_capacity), 
  //                       "Alpha_High", DoubleValue (alpha_high), "Alpha_Low", DoubleValue (alpha_low));
  tch.SetRootQueueDisc ("ns3::RedQueueDisc", "MaxSize", StringValue (queue_capacity));
  
  QueueDiscContainer qdiscs = tch.Install (rightrouterdevices);  // install TrafficControlHelper on the right router-side NetDevices
  // QueueDiscContainer qdiscs = tch.Install (routerdevices);



///////////////some examples for Multi class queue discs///////////////
  // from: wifi-ac-mapping-test-suite.cc
  // TrafficControlHelper tch;
  // uint16_t handle = tch.SetRootQueueDisc("ns3::MqQueueDisc");
  // TrafficControlHelper::ClassIdList cls =
  //     tch.AddQueueDiscClasses(handle, 4, "ns3::QueueDiscClass");
  // tch.AddChildQueueDiscs(handle, cls, "ns3::FqCoDelQueueDisc");
  // tch.Install(apDev);
  // tch.Install(staDev);

  // an example that uses an actual dumabell helper: red-vs-fengadaptive.cc
  // TrafficControlHelper tchBottleneck;
  // QueueDiscContainer queueDiscs;
  // tchBottleneck.SetRootQueueDisc("ns3::RedQueueDisc");
  // tchBottleneck.Install(d.GetLeft()->GetDevice(0));
  // queueDiscs = tchBottleneck.Install(d.GetRight()->GetDevice(0));
///////////////////////////////////////////////////////////////////////

  Ptr<QueueDisc> q0 = qdiscs.Get (0); // look at the router queue
  Ptr<QueueDisc> q1 = qdiscs.Get (1); // look at the router queue
  // Ptr<QueueDisc> q2 = qdiscs.Get (2); // look at the router queue
  // The Next Line Displayes "PacketsInQueue" statistic at the Traffic Controll Layer
  q0->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&TcPacketsInQueue0_Trace));
  q1->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&TcPacketsInQueue1_Trace));
  // q2->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&TcPacketsInQueue2_Trace));

/////////////////////////////////////////for DT or FB QueueDiscs////////////////////////////////////////////////////////////////////////
  // q->TraceConnectWithoutContext ("HighPriorityPacketsInQueue", MakeCallback (&TcHighPriorityPacketsInQueueTrace));  // ### ADDED BY ME #####
  // q->TraceConnectWithoutContext ("LowPriorityPacketsInQueue", MakeCallback (&TcLowPriorityPacketsInQueueTrace));  // ### ADDED BY ME #####
  // q->TraceConnectWithoutContext("EnqueueingThreshold_High", MakeCallback (&QueueThresholdHighTrace)); // ### ADDED BY ME #####
  // q->TraceConnectWithoutContext("EnqueueingThreshold_Low", MakeCallback (&QueueThresholdLowTrace)); // ### ADDED BY ME #####
  // // q->TraceConnectWithoutContext("DropBeforeEnqueue", MakeCallback (&DropBeforeEnqueueTrace)); // ### ADDED BY ME #####
  // Config::ConnectWithoutContextFailSafe ("/NodeList/1/$ns3::TrafficControlLayer/RootQueueDiscList/0/SojournTime",
  //                                MakeCallback (&SojournTimeTrace));                          
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // assign ipv4 addresses (ipv6 addresses apparently are still not fully
  // supported by ns3)
  ns3::Ipv4AddressHelper routerips =
      ns3::Ipv4AddressHelper("10.3.0.0", "255.255.255.0");
  ns3::Ipv4AddressHelper leftips =
      ns3::Ipv4AddressHelper("10.1.0.0", "255.255.255.0");
  ns3::Ipv4AddressHelper rightips =
      ns3::Ipv4AddressHelper("10.2.0.0", "255.255.255.0");

  Ipv4InterfaceContainer routerifs;
  Ipv4InterfaceContainer leftleafifs;
  Ipv4InterfaceContainer leftrouterifs;
  Ipv4InterfaceContainer rightleafifs;
  Ipv4InterfaceContainer rightrouterifs;
  
  // assign addresses to connection connecting routers
  routerifs = routerips.Assign(routerdevices);

    // assign addresses to connection between routers and leaves
    for (uint32_t i = 0; i<number_of_clients; ++i) 
    {
      // Assign to left side
      NetDeviceContainer ndcleft;
      ndcleft.Add(leftleafdevices.Get(i));
      ndcleft.Add(leftrouterdevices.Get(i));
      Ipv4InterfaceContainer ifcleft = leftips.Assign(ndcleft);
      leftleafifs.Add(ifcleft.Get(0));
      leftrouterifs.Add(ifcleft.Get(1));
      leftips.NewNetwork();
      // Assign to right side
      NetDeviceContainer ndcright;
      ndcright.Add(rightleafdevices.Get(i));
      ndcright.Add(rightrouterdevices.Get(i));
      Ipv4InterfaceContainer ifcright = rightips.Assign(ndcright);
      rightleafifs.Add(ifcright.Get(0));
      rightrouterifs.Add(ifcright.Get(1));
      rightips.NewNetwork();
  }

  // and setup ip routing tables to get total ip-level connectivity.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t servPort = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), servPort));
  // Create a packet sink to receive these packets on n2
  PacketSinkHelper sink (socketType, sinkLocalAddress); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
  ApplicationContainer sinkApps, sourceApps;
  
  // create all the source and sink apps
  for (size_t i = 0; i<number_of_clients; ++i) 
  {
    // create sockets
    ns3::Ptr<ns3::Socket> sockptr;

    if (transportProt.compare("TCP") == 0) 
    {
      // setup source socket
      sockptr =
          ns3::Socket::CreateSocket(leftleaves.Get(i),
                  ns3::TcpSocketFactory::GetTypeId());
      ns3::Ptr<ns3::TcpSocket> tcpsockptr =
          ns3::DynamicCast<ns3::TcpSocket> (sockptr);
    } 
    else if (transportProt.compare("UDP") == 0) 
    {
      // setup source socket
      sockptr =
          ns3::Socket::CreateSocket(leftleaves.Get(i),
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
    // setup sink
    // sinkApp = sink.Install (rightleaves.Get(i));
    sinkApps.Add(sink.Install (rightleaves.Get(i)));
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (simulationTime + 0.1));

    uint32_t payloadSize = 1024;
    uint32_t numOfPackets = 100;  // number of packets to send in one stream for custom application
    // create and install Client apps:    
    if (applicationType.compare("standardClient") == 0) 
    {
      // Install UDP application on the sender  
      UdpClientHelper clientHelper (rightleafifs.GetAddress(i), servPort);
      clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
      clientHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      sourceApps.Add(clientHelper.Install (leftleaves.Get(i)));
      sourceApps.Start (Seconds (1.0));
      sourceApps.Stop (Seconds(3.0));
    } 
    else if (applicationType.compare("OnOff") == 0) 
    {
      // Create the OnOff applications to send TCP/UDP to the server
      InetSocketAddress socketAddressUp = InetSocketAddress (rightleafifs.GetAddress(i), servPort);
      OnOffHelper clientHelper (socketType, Address ());
      clientHelper.SetAttribute ("Remote", AddressValue (socketAddressUp));
      clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
      clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
      clientHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      clientHelper.SetAttribute ("DataRate", StringValue ("2Mb/s"));
      sourceApps.Add(clientHelper.Install (leftleaves.Get(i)));
      sourceApps.Start (Seconds (1.0));
      sourceApps.Stop (Seconds(3.0));
    } 
    else if (applicationType.compare("customOnOff") == 0) 
    {
      // Create the Custom application to send TCP/UDP to the server
      
      InetSocketAddress socketAddressUp = InetSocketAddress (rightleafifs.GetAddress(i), servPort);
      Ptr<CustomOnOffApplication> customOnOffApp = CreateObject<CustomOnOffApplication> ();
      customOnOffApp->Setup(sockptr);
      customOnOffApp->SetAttribute("Remote", AddressValue (socketAddressUp));
      customOnOffApp->SetAttribute("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
      customOnOffApp->SetAttribute("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
      customOnOffApp->SetAttribute("PacketSize", UintegerValue (payloadSize));
      customOnOffApp->SetAttribute("DataRate", StringValue ("2Mb/s"));
      customOnOffApp->SetAttribute("EnableSeqTsSizeHeader", BooleanValue (false));
      leftleaves.Get(i)->AddApplication(customOnOffApp);
      customOnOffApp->SetStartTime (Seconds (1.0));
      customOnOffApp->SetStopTime (Seconds(3.0));
    }
    else if (applicationType.compare("customApplication") == 0)
    {
      // Create the Custom application to send TCP/UDP to the server
      
      Ptr<TutorialApp> customApp = CreateObject<TutorialApp> ();
      InetSocketAddress socketAddressUp = InetSocketAddress (rightleafifs.GetAddress(i), servPort);  // sink IpV4 Address
      customApp->Setup (sockptr, socketAddressUp, payloadSize, numOfPackets, DataRate ("1Mbps"));
      leftleaves.Get(i)->AddApplication (customApp);
      customApp->SetStartTime (Seconds (1.0));
      customApp->SetStopTime (Seconds(3.0));
    } 
    else 
    {
      std::cerr << "unknown app type: " << applicationType << std::endl;
      exit(1);
    }
  }

  
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  std::string dirToSave = "mkdir -p " + dir + queue_disc_type;
  if (system (dirToSave.c_str ()) == -1)
    {
      exit (1);
    }  

  Simulator::Stop (Seconds (simulationTime + 10));
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl << "Topology: Dumbell" << std::endl;
  std::cout << std::endl << "Queueing Algorithm: " + queue_disc_type << std::endl;
  // std::cout << std::endl << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
  std::cout << std::endl << "Application: " + applicationType << std::endl;

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
// a loop to sum the Tx/Rx Packets/Bytes from all nodes
  uint32_t txPackets = 0; 
  uint64_t txBytes = 0;
  uint32_t rxPackets = 0; 
  uint64_t rxBytes = 0;
  for (size_t i = 1; i <= stats.size(); i++)
  // stats indexing needs to start from 1
  {
    txPackets = txPackets + stats[i].txPackets;
    txBytes = txBytes + stats[i].txBytes;
    rxPackets = rxPackets + stats[i].rxPackets;
    rxBytes = rxBytes + stats[i].rxBytes;
  }

  std::cout << "  Tx Packets/Bytes:   " << txPackets
            << " / " << txBytes << std::endl;
  std::cout << "  Rx Packets/Bytes:   " << rxPackets
            << " / " << rxBytes << std::endl;

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
  for (size_t i = 0; i < qdiscs.GetN(); i++)
  {
    std::cout << "Queue Disceplene " << i << ":" << std::endl;
    std::cout << qdiscs.Get(i)->GetStats () << std::endl;
  }
  
  // Added to create a .txt file with the summary of the tested scenario statistics
  std::ofstream testFlowStatistics (dir + queue_disc_type + "/Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: Line" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + queue_disc_type << std::endl;
  // testFlowStatistics << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
  testFlowStatistics << "Application: " + applicationType << std::endl; 
  testFlowStatistics << std::endl << "*** Flow monitor statistics ***" << std::endl;
  testFlowStatistics << "  Tx Packets/Bytes:   " << txPackets << " / " << txBytes << std::endl;
  testFlowStatistics << "  Rx Packets/Bytes:   " << rxPackets << " / " << rxBytes << std::endl;
  testFlowStatistics << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc
                     << " / " << bytesDroppedByQueueDisc << std::endl;
  testFlowStatistics << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice
                     << " / " << bytesDroppedByNetDevice << std::endl;
  testFlowStatistics << "  Throughput: " << TpT << " Mbps" << std::endl;                   
  testFlowStatistics << std::endl << "*** TC Layer statistics ***" << std::endl;
  for (size_t i = 0; i < qdiscs.GetN(); i++)
  {
    testFlowStatistics << "Queue Disceplene " << i << ":" << std::endl;
    testFlowStatistics << qdiscs.Get(i)->GetStats () << std::endl;
  }
  testFlowStatistics.close ();

  // command line needs to be in ./scratch/ inorder for the script to produce gnuplot correctly///
  // system (("gnuplot " + dir + "gnuplotScriptTcHighPriorityPacketsInQueue").c_str ());

  Simulator::Destroy ();
  return 0;
}