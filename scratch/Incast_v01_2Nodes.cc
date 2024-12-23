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
//                  
//               n0                                .
//               .\                                .
//               . \ 10Mb/s, 10ms                  .
//               .  \    10Kb/s, 10ms              .
//               .   R-----------------S           .
//               .  /                              .
//               . / 10Mb/s, 10ms                  . 
//               ./                                .
//               n1                                .
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
#include <numeric>
#include <vector>
#include <utility>
#include <list>
#include <array>
#include <filesystem>

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
#include "ns3/tutorial-app.h"
#include "ns3/custom_onoff-application.h"
#include "ns3/stats-module.h"

using namespace ns3;

std::string dir = "./Trace_Plots/Incast_Topology/";
std::string queue_disc_type = "FifoQueueDisc"; // "DT_FifoQueueDisc_v02"/"FB_FifoQueueDisc_v01"/"FifoQueueDisc"

uint32_t prev = 0;
Time prevTime = Seconds (0);

NS_LOG_COMPONENT_DEFINE ("Traffic_Control_Example_Incast_Topology_v01");

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
  plot.SetLegend("Time[sec]", "PacketsInQueue");
  
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
TcPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream piq (dir + queue_disc_type + "/highPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  piq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  piq.close ();
  std::cout << "TcPacketsInQueue " << newValue << std::endl;
}

// Trace the number of High Priority packets in the Queue
void
TcHighPriorityPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream hppiq (dir + queue_disc_type + "/highPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  hppiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  hppiq.close ();

  std::cout << "TcHighPriorityPacketsInQueue " << newValue << std::endl;
}

// Trace the number of Low Priority packets in the Queue
void
TcLowPriorityPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::ofstream lppiq (dir + queue_disc_type + "/lowPriorityPacketsInQueueTrace.dat", std::ios::out | std::ios::app);
  lppiq << Simulator::Now ().GetSeconds () << " " << newValue << std::endl;
  lppiq.close ();

  std::cout << "TcLowPriorityPacketsInQueue " << newValue << std::endl;
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

void
DevicePacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout <<Simulator::Now ().GetSeconds () << " DevicePacketsInQueue: " << newValue << std::endl;
}

// Function to check queue length of Router 1
void
CheckQueueSize(Ptr<Queue<Packet>> queue)
{
    uint32_t qSize = queue->GetCurrentSize().GetValue();

    // Check queue size every 1/100 of a second
    Simulator::Schedule(Seconds(0.001), &CheckQueueSize, queue);
    std::ofstream fPlotQueue(std::stringstream(dir + queue_disc_type + "/queue-size.dat").str(),
                             std::ios::out | std::ios::app);
    fPlotQueue << Simulator::Now().GetSeconds() << " " << qSize << std::endl;
    fPlotQueue.close();
}

// to monitor the OnOff state of a single OnOff Application in real time
void
CheckOnOffState(Ptr<CustomOnOffApplication> customOnOffApp)
{
    bool appState = customOnOffApp->GetCurrentState();

    // Check OnOff state every 1/100 of a second
    Simulator::Schedule(Seconds(0.001), &CheckOnOffState, customOnOffApp);
    // for testing:
    // std::cout <<Simulator::Now ().GetSeconds () << " OnOff State: " << appState << std::endl;

    // save to .dat file
    std::ofstream fPlotOnOffState(std::stringstream(dir + queue_disc_type + "/OnOffStateTrace.dat").str(),
                             std::ios::out | std::ios::app);
    fPlotOnOffState << Simulator::Now().GetSeconds() << " " << appState << std::endl;
    fPlotOnOffState.close();
}

// to calculate the OnOff traffic generated by all OnOff applications in real time
void
returnOnOffTraffic(Ptr<CustomOnOffApplication> customOnOffApp1, Ptr<CustomOnOffApplication> customOnOffApp2, double maxDuration, double startTime)
{
    double elapsedTime = Simulator::Now().GetSeconds() - startTime;

    // Stop monitoring if the elapsed time exceeds maxDuration
    if (elapsedTime > maxDuration) {
        return; // Stop the monitoring
    }
    
    bool appState1 = customOnOffApp1->GetCurrentState();
    bool appState2 = customOnOffApp2->GetCurrentState();
    int traffic = 1 * appState1 + 1 * appState2;

    // Check OnOff state every 1/100 of a second
    Simulator::Schedule(Seconds(0.001), &returnOnOffTraffic, customOnOffApp1, customOnOffApp2, maxDuration, startTime);
    
    // for testing:
    // std::cout <<Simulator::Now ().GetSeconds () << " OnOff State: " << appState << std::endl;

    // save to .dat file
    std::ofstream fPlotOnOffTrafficState(std::stringstream(dir + queue_disc_type + "/generatedOnOffTrafficTrace.dat").str(),
                             std::ios::out | std::ios::app);
    fPlotOnOffTrafficState << Simulator::Now().GetSeconds() << " " << traffic << std::endl;
    fPlotOnOffTrafficState.close();
}


// Original example
// void
// CheckQueueSize(Ptr<QueueDisc> queue)
// {
//     uint32_t qSize = queue->GetCurrentSize().GetValue();

//     // Check queue size every 1/100 of a second
//     Simulator::Schedule(Seconds(0.001), &CheckQueueSize, queue);
//     std::ofstream fPlotQueue(std::stringstream(dir + "queue-size.dat").str(),
//                              std::ios::out | std::ios::app);
//     fPlotQueue << Simulator::Now().GetSeconds() << " " << qSize << std::endl;
//     fPlotQueue.close();
// }

void
SojournTimeTrace (Time sojournTime)
{
  std::cout << "Sojourn time " << sojournTime.ToDouble (Time::MS) << "ms" << std::endl;
}

static void
OnOffAppTxTrace(Ptr<const Packet> tx_packet)
{
    std::cout << Simulator::Now().GetSeconds() << "\t" << tx_packet
                         << std::endl;
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
  cmd.AddValue ("applicationType", "Application type to use to send data: OnOff, standardClient", applicationType);
  cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, Udp", transportProt);
  cmd.Parse (argc, argv);

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
  
  // LogComponentEnable("PacketSink", LOG_LEVEL_INFO); 

  // Here, we will explicitly create three nodes.  The first container contains
  // nodes 0 and 1 from the diagram above, and the second one contains nodes
  // 1 and 2.  This reflects the channel connectivity, and will be used to
  // install the network interfaces and connect them with a channel.
  NodeContainer clientNodes;
  clientNodes.Create (3);

  NodeContainer serverNodes;
  serverNodes.Add (clientNodes.Get (1));
  serverNodes.Create (1);

  NodeContainer allNodes = NodeContainer (serverNodes, clientNodes.Get(0), clientNodes.Get(2));

  // We create the channels first without any IP addressing information
  // First make and configure the helper, so that it will put the appropriate
  // attributes on the network interfaces and channels we are about to install.
  
  // Create the point-to-point link helpers
  PointToPointHelper p2p1_l;  // the link between each sender to Router
  p2p1_l.SetDeviceAttribute  ("DataRate", StringValue ("10Mbps"));
  p2p1_l.SetChannelAttribute ("Delay", StringValue ("5ms"));
  p2p1_l.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1000p"));

  PointToPointHelper p2p1_h;  // the link between each sender to Router
  p2p1_h.SetDeviceAttribute  ("DataRate", StringValue ("10Mbps"));
  p2p1_h.SetChannelAttribute ("Delay", StringValue ("5ms"));

  PointToPointHelper p2p2;  // the link between router and Reciever
  p2p2.SetDeviceAttribute  ("DataRate", StringValue ("1Mbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("10ms"));
  // minimal value for NetDevice buffer is 1p. we set it in order to observe Traffic Controll effects only.
  p2p2.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));


  // And then install devices and channels connecting our topology.
  NetDeviceContainer sender1 = p2p1_l.Install (clientNodes.Get(0), clientNodes.Get(1));
  NetDeviceContainer sender2 = p2p1_h.Install (clientNodes.Get(2), clientNodes.Get(1));
  // NetDeviceContainer sender3 = p2p1.Install (senders.Get(3), senders.Get(1));
  NetDeviceContainer reciever = p2p2.Install (serverNodes);

  // trace Tx packets in Net device:
  Ptr<NetDevice> nd1 = sender1.Get(0);
  Ptr<PointToPointNetDevice> ptpnd1 = DynamicCast<PointToPointNetDevice>(nd1);
  Ptr<Queue<Packet>> queue1 = ptpnd1->GetQueue();

  // Simulator::ScheduleNow (&CheckQueueSize, queue1); // 
  // queue1->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&DevicePacketsInQueueTrace));

  // Now add ip/tcp stack to all nodes.
  InternetStackHelper internet;
  internet.InstallAll ();

  TrafficControlHelper tch;
  // tch.SetRootQueueDisc ("ns3::RedQueueDisc", "MaxSize", StringValue ("5p"));
  tch.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue ("20p"));
  // tch.SetRootQueueDisc ("ns3::" + queue_disc_type, "MaxSize", StringValue (queue_capacity), 
                        // "Alpha_High", DoubleValue (alpha_high), "Alpha_Low", DoubleValue (alpha_low));

  QueueDiscContainer qdiscs = tch.Install (reciever);

  // Ptr<QueueDisc> q = qdiscs.Get (1); // original code - doesn't show values
  Ptr<QueueDisc> q = qdiscs.Get (0); // look at the router queue - shows actual values
  // The Next Line Displayes "PacketsInQueue" statistic at the Traffic Controll Layer
  // q->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&TcPacketsInQueueTrace));
  // q->TraceConnectWithoutContext ("HighPriorityPacketsInQueue", MakeCallback (&TcHighPriorityPacketsInQueueTrace));  // ### ADDED BY ME #####
  // q->TraceConnectWithoutContext ("LowPriorityPacketsInQueue", MakeCallback (&TcLowPriorityPacketsInQueueTrace));  // ### ADDED BY ME #####
  // q->TraceConnectWithoutContext("EnqueueingThreshold_High", MakeCallback (&QueueThresholdHighTrace)); // ### ADDED BY ME #####
  // q->TraceConnectWithoutContext("EnqueueingThreshold_Low", MakeCallback (&QueueThresholdLowTrace)); // ### ADDED BY ME #####
  Config::ConnectWithoutContextFailSafe ("/NodeList/1/$ns3::TrafficControlLayer/RootQueueDiscList/0/SojournTime",
                                 MakeCallback (&SojournTimeTrace));
  
  // Ptr<NetDevice> nd = serverDevice.Get (1);  // original value
  // Ptr<NetDevice> nd = reciever.Get (0);  //router side? fits queue-discs-benchmark example
  // Ptr<PointToPointNetDevice> ptpnd = DynamicCast<PointToPointNetDevice> (nd);
  // Ptr<Queue<Packet> > queue = ptpnd->GetQueue ();
  // The Next Line Displayes "PacketsInQueue" statistic at the NetDevice Layer
  // queue->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&DevicePacketsInQueueTrace));


  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  ipv4.NewNetwork ();
  Ipv4InterfaceContainer sender1Interface = ipv4.Assign (sender1);
  ipv4.NewNetwork ();
  Ipv4InterfaceContainer sender2Interface = ipv4.Assign (sender2);
  // ipv4.NewNetwork ();
  // Ipv4InterfaceContainer sender3Interface = ipv4.Assign (sender3);
  ipv4.NewNetwork ();
  Ipv4InterfaceContainer routerInterface = ipv4.Assign (reciever);

  // and setup ip routing tables to get total ip-level connectivity.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t servPort = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), servPort));
  // Create a packet sink to receive these packets on n2
  PacketSinkHelper sink (socketType, sinkLocalAddress);                       
  ApplicationContainer sinkApp = sink.Install (serverNodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime + 0.1));
  
  uint32_t payloadSize = 1024;
  uint32_t numOfPackets = 100;  // number of packets to send in one stream for custom application

  // Install application on the senders
  //get the address of the reciever node NOT THE ROUTER!!!
  if (applicationType.compare("standardClient") == 0)
  {
    UdpClientHelper udpClient (routerInterface.GetAddress(1), servPort);
    udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (payloadSize));

    ApplicationContainer sourceApps1 = udpClient.Install (clientNodes.Get (0));
    sourceApps1.Start (Seconds (1.0));
    sourceApps1.Stop (Seconds(3.0));
    ApplicationContainer sourceApps2 = udpClient.Install (clientNodes.Get (2));
    sourceApps2.Start (Seconds (1.0));
    sourceApps2.Stop (Seconds(3.0));
  }
  else if (applicationType.compare("OnOff") == 0)
  {
   // Create the OnOff applications to send TCP/UDP to the server
    InetSocketAddress socketAddressUp = InetSocketAddress (routerInterface.GetAddress(1), servPort);
    
    OnOffHelper clientHelper (socketType, Address ());
    
    clientHelper.SetAttribute ("Remote", AddressValue (socketAddressUp));
    clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
    clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    clientHelper.SetAttribute ("DataRate", StringValue ("1Mb/s"));
    
  
    ApplicationContainer sourceApps1 = clientHelper.Install (clientNodes.Get (0));
    sourceApps1.Start (Seconds (1.0));
    sourceApps1.Stop (Seconds(3.0));

    ApplicationContainer sourceApps2 = clientHelper.Install (clientNodes.Get (2));
    sourceApps2.Start (Seconds (1.0));
    sourceApps2.Stop (Seconds(3.0));
  }
  else if (applicationType.compare("customOnOff") == 0)
  {
    // Create the Custom application to send TCP/UDP to the server
    Ptr<Socket> ns3UdpSocket1 = Socket::CreateSocket (clientNodes.Get (0), UdpSocketFactory::GetTypeId ());
    Ptr<Socket> ns3UdpSocket2 = Socket::CreateSocket (clientNodes.Get (2), UdpSocketFactory::GetTypeId ());
 
    // ns3UdpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
    
    InetSocketAddress socketAddressUp = InetSocketAddress (routerInterface.GetAddress(1), servPort); // sink IPv4 address
    
    Ptr<CustomOnOffApplication> customOnOffApp1 = CreateObject<CustomOnOffApplication> ();
    customOnOffApp1->Setup(ns3UdpSocket1);
    customOnOffApp1->SetAttribute("Remote", AddressValue (socketAddressUp));
    customOnOffApp1->SetAttribute("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
    customOnOffApp1->SetAttribute("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
    customOnOffApp1->SetAttribute("PacketSize", UintegerValue (payloadSize));
    customOnOffApp1->SetAttribute("DataRate", StringValue ("2Mb/s"));
    customOnOffApp1->SetAttribute("EnableSeqTsSizeHeader", BooleanValue (false));
    customOnOffApp1->SetStartTime (Seconds (1.0));
    customOnOffApp1->SetStopTime (Seconds(3.0));
    clientNodes.Get (0)->AddApplication (customOnOffApp1);
    // customOnOffApp1->TraceConnectWithoutContext ("Tx", MakeCallback (&OnOffAppTxTrace));
    // Simulator::ScheduleNow (&CheckOnOffState, customOnOffApp1); // to return OnOff state of the application in real time

    Ptr<CustomOnOffApplication> customOnOffApp2 = CreateObject<CustomOnOffApplication> ();
    customOnOffApp2->Setup(ns3UdpSocket2);
    customOnOffApp2->SetAttribute("Remote", AddressValue (socketAddressUp));
    customOnOffApp2->SetAttribute("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.05]"));
    customOnOffApp2->SetAttribute("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.05]"));
    customOnOffApp2->SetAttribute("PacketSize", UintegerValue (payloadSize));
    customOnOffApp2->SetAttribute("DataRate", StringValue ("2Mb/s"));
    customOnOffApp2->SetAttribute("EnableSeqTsSizeHeader", BooleanValue (false));
    customOnOffApp2->SetStartTime (Seconds (1.0));
    /////////////////////
    // Hook trace source after application starts
    // Simulator::Schedule (Seconds (1.0) + MilliSeconds (1), &TraceCwnd, 0, 0);
    ////////////////////////////
    customOnOffApp2->SetStopTime (Seconds(3.0));
    clientNodes.Get (2)->AddApplication (customOnOffApp2);

    //monitor OnOff Traffic generated from the OnOff Applications:
    Simulator::Schedule (Seconds (1.0), &returnOnOffTraffic, customOnOffApp1, customOnOffApp2, 3.1, Simulator::Now().GetSeconds());
  }

  else if (applicationType.compare("customApplication") == 0)
  {
    // Create the Custom application to send TCP/UDP to the server
    Ptr<Socket> ns3UdpSocket1 = Socket::CreateSocket (clientNodes.Get (0), UdpSocketFactory::GetTypeId ());
    Ptr<Socket> ns3UdpSocket2 = Socket::CreateSocket (clientNodes.Get (2), UdpSocketFactory::GetTypeId ());

    InetSocketAddress socketAddressUp = InetSocketAddress (routerInterface.GetAddress(1), servPort);  // sink IPv4 address

    Ptr<TutorialApp> customApp1 = CreateObject<TutorialApp> ();
    customApp1->Setup (ns3UdpSocket1, socketAddressUp, payloadSize, numOfPackets, DataRate ("1Mbps"));
    customApp1->SetStartTime (Seconds (1.0));
    customApp1->SetStopTime (Seconds(3.0));
    clientNodes.Get (0)->AddApplication (customApp1);

    Ptr<TutorialApp> customApp2 = CreateObject<TutorialApp> ();
    customApp2->Setup (ns3UdpSocket2, socketAddressUp, payloadSize, numOfPackets, DataRate ("1Mbps"));
    customApp2->SetStartTime (Seconds (1.0));
    /////////////////////
    // Hook trace source after application starts
    // Simulator::Schedule (Seconds (1.0) + MilliSeconds (1), &TraceCwnd, 0, 0);
    ////////////////////////////
    customApp2->SetStopTime (Seconds(3.0));
    clientNodes.Get (2)->AddApplication (customApp2);
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a new directory to store the output of the program
  std::string dirToSave = dir + queue_disc_type + "/";
  if (!((std::filesystem::exists(dirToSave)) && (std::filesystem::is_directory(dirToSave))))
  {
    system (("mkdir -p " + dirToSave).c_str ());
  } 

  // Generate PCAP traces if it is enabled
  if (enablePcap)
    {
      // NS_LOG_INFO ("Enable pcap tracing.");
      if (system ((dirToSave + "pcap/").c_str ()) == -1)
        {
          exit (1);
        }
      p2p2.EnablePcapAll (dir + queue_disc_type + "/pcap/dt", true);
    }

  Simulator::Stop (Seconds (simulationTime + 10));
  Simulator::Run ();

  // print the tested scenario at the top of the terminal: Topology, Queueing Algorithm and Application.
  std::cout << std::endl << "Topology: Incast" << std::endl;
  std::cout << std::endl << "Queueing Algorithm: " + queue_disc_type << std::endl;
  std::cout << std::endl << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
  std::cout << std::endl << "Application: " + applicationType << std::endl;

  // monitor->SerializeToXmlFile("myTrafficControl_IncastTopology_v01_1.xml", true, true);

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
// a loop to sum the Tx/Rx Packets/Bytes from all nodes
  uint32_t txPackets = 0; 
  uint64_t txBytes = 0;
  uint32_t rxPackets = 0; 
  uint64_t rxBytes = 0;
  for (size_t i = 1; i <= stats.size(); i++)
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

  std::cout << std::endl << "*** Application statistics ***" << std::endl;
  double goodTpT = 0;

  uint64_t totalBytesRx = 0;
  for (size_t i = 0; i < sinkApp.GetN(); i++)
  {
    totalBytesRx = totalBytesRx + DynamicCast<PacketSink> (sinkApp.Get (i))->GetTotalRx ();
  }
  goodTpT = totalBytesRx * 8 / (simulationTime * 1000000.0); //Mbit/s
  std::cout << "  Rx Bytes: " << totalBytesRx << std::endl;
  std::cout << "  Average Goodput: " << goodTpT << " Mbit/s" << std::endl;

  std::cout << std::endl << "*** TC Layer statistics ***" << std::endl;
  std::cout << q->GetStats () << std::endl;

  // Added to create a .txt file with the summary of the tested scenario statistics
  std::ofstream testFlowStatistics (dirToSave + "Statistics.txt", std::ios::out | std::ios::app);
  testFlowStatistics << "Topology: Line" << std::endl;
  testFlowStatistics << "Queueing Algorithm: " + queue_disc_type << std::endl;
  testFlowStatistics << "Alpha High = " << alpha_high << " Alpha Low = " << alpha_low <<std::endl;
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
  testFlowStatistics << q->GetStats () << std::endl;
  testFlowStatistics.close ();

  // MOVE ALL TRACE/.dat files to the run directory:
  // system (("mv -f " + dir + "/*.dat " + dirToSave).c_str ());
  // system (("mv -f " + dir + "/*.txt " + dirToSave).c_str ());

  // Create the gnuplot.//////////////////////////////
  generate1DGnuPlotFromDatFile(dirToSave + "generatedOnOffTrafficTrace.dat");

  Simulator::Destroy ();
  return 0;
}