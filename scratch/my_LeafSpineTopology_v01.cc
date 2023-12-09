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
 * Spine-Leaf Topology, with 2 Spines, and 2 Leaf Switches, and N Servers connected to each Leaf
 *
 *            S0      S1
 *            | \    / |
 *            |  \  /  |
 *            |   \/   |
 *            |   /\   |
 *            |  /  \  |
 *            | /    \ |
 *            L0      L1
 *           /|\      /|\
 *          /   \    /   \ 
 *         servers  servers
 * 
 * This script includes plotting!
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
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
// #include "ns3/link-monitor-module.h"

#include "ns3/gnuplot.h"


// There are 40 servers connecting to each leaf switch
#define SERVER_COUNT 40
#define SPINE_COUNT 2
#define LEAF_COUNT 2

#define SPINE_LEAF_CAPACITY  10000000000          // 10Gbps
#define LEAF_SERVER_CAPACITY 100000000000         // 100Gbps
#define LINK_LATENCY MicroSeconds(100)            // RTT = 200 MicroSeconds, latency = RTT/2 = 100(?) MicroSeconds 
#define BUFFER_SIZE 1000                          // 1000 Packets

// #define RED_QUEUE_MARKING 65 			  // 65 Packets (available only in DcTcp)

// The simulation starting and ending time
#define START_TIME 0.0
#define END_TIME 30

// The flow port range, each flow will be assigned a random port number within this range
#define FLOW_A_PORT 5000
#define FLOW_B_PORT 6000
#define SERV_PORT 50000

// Adopted from the simulation from snowzjx
// Acknowledged to https://github.com/snowzjx/ns3-load-balance.git
#define PACKET_SIZE 1024 // 1024 [bytes]

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SpineLeafFlow");

Gnuplot2dDataset throughputDatasetA;
Gnuplot2dDataset throughputDatasetB;

uint64_t flowARecvBytes = 0;
uint64_t flowBRecvBytes = 0;

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

void
CheckFlowAThroughput (Ptr<PacketSink> sink)
{
  uint32_t totalRecvBytes = sink->GetTotalRx ();
  uint32_t currentPeriodRecvBytes = totalRecvBytes - flowARecvBytes;

  flowARecvBytes = totalRecvBytes;

  Simulator::Schedule (Seconds (0.0001), &CheckFlowAThroughput, sink);

  NS_LOG_UNCOND ("Throughput of Flow A: " << currentPeriodRecvBytes * 8 / 0.0001 << "bps");

  throughputDatasetA.Add (Simulator::Now().GetSeconds (), currentPeriodRecvBytes * 8 / 0.0001);
}

void
CheckFlowBThroughput (Ptr<PacketSink> sink)
{
  uint32_t totalRecvBytes = sink->GetTotalRx ();
  uint32_t currentPeriodRecvBytes = totalRecvBytes - flowBRecvBytes;

  flowBRecvBytes = totalRecvBytes;

  Simulator::Schedule (Seconds (0.0001), &CheckFlowBThroughput, sink);

  NS_LOG_UNCOND ("Throughput of Flow B: " << currentPeriodRecvBytes * 8 / 0.0001 << "bps");

  throughputDatasetB.Add (Simulator::Now().GetSeconds (), currentPeriodRecvBytes * 8 / 0.0001);
}

void
DoGnuPlot (uint32_t flowletTimeout)
{
    Gnuplot flowAThroughputPlot (StringCombine ("flow_A_", ToString (flowletTimeout), "_throughput.png"));
    flowAThroughputPlot.SetTitle ("Flow A Throughput");
    flowAThroughputPlot.SetTerminal ("png");
    flowAThroughputPlot.AddDataset (throughputDatasetA);
    std::ofstream flowAThroughputFile (StringCombine ("flow_A_", ToString (flowletTimeout), "_throughput.plt").c_str ());
    flowAThroughputPlot.GenerateOutput (flowAThroughputFile);
    flowAThroughputFile.close ();

    Gnuplot flowBThroughputPlot (StringCombine ("flow_B_", ToString (flowletTimeout), "_throughput.png"));
    flowBThroughputPlot.SetTitle ("Flow B Throughput");
    flowBThroughputPlot.SetTerminal ("png");
    flowBThroughputPlot.AddDataset (throughputDatasetB);
    std::ofstream flowBThroughputFile (StringCombine ("flow_B_", ToString (flowletTimeout), "_throughput.plt").c_str ());
    flowBThroughputPlot.GenerateOutput (flowBThroughputFile);
    flowBThroughputFile.close ();
}

// std::string
// DefaultFormat (struct LinkProbe::LinkStats stat)
// {
//   std::ostringstream oss;
//   //oss << stat.txLinkUtility << "/"
//       //<< stat.packetsInQueue << "/"
//       //<< stat.bytesInQueue << "/"
//       //<< stat.packetsInQueueDisc << "/"
//       //<< stat.bytesInQueueDisc;
//   /*oss << stat.packetsInQueue;*/
//   oss << stat.txLinkUtility << ",";
//   oss << stat.packetsInQueueDisc;
//   return oss.str ();
// }

int main (int argc, char *argv[])
{
#if 1
    LogComponentEnable ("SpineLeafFlow", LOG_LEVEL_INFO);
#endif

    std::string applicationType = "standardClient"; // "standardClient"/"OnOff"/"customApplication"/"customOnOff"
    // Command line parameters parsing
    std::string transportProt = "UDP"; // "UDP"/"TCP"
    std::string socketType;
    uint32_t flowletTimeout = 50;

    CommandLine cmd;
    cmd.AddValue ("transportProt", "Transport protocol to use: Udp, Tcp, DcTcp", transportProt);
    cmd.AddValue ("flowletTimeout", "Conga flowlet timeout", flowletTimeout);
    cmd.Parse (argc, argv);

    NS_LOG_INFO ("Config parameters");
    if (transportProt.compare("Tcp")==0)
    {
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(PACKET_SIZE));
        Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue (0));
        Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (MilliSeconds (5)));
        Config::SetDefault ("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue (10 * PACKET_SIZE));
        Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
        Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue(MilliSeconds(5)));
        Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (100)));
        Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MicroSeconds (160)));
        Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (160000000));
        Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (160000000));
    }
    
    NS_LOG_INFO ("Create nodes");
    NodeContainer spines;
    spines.Create (SPINE_COUNT);
    NodeContainer leaves;
    leaves.Create (LEAF_COUNT);
    NodeContainer servers;
    servers.Create (SERVER_COUNT * LEAF_COUNT);

    NS_LOG_INFO ("Install Internet stacks");
    InternetStackHelper internet;
    Ipv4StaticRoutingHelper staticRoutingHelper;

	internet.SetRoutingHelper (staticRoutingHelper);
    internet.Install (servers);
    internet.Install (spines);
    internet.Install (leaves);

    NS_LOG_INFO ("Install channels and assign addresses");

    PointToPointHelper p2p;
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer leaf2spineInterfaceContainer, serv2leafInterfaceContainer;

    TrafficControlHelper tch;
    tch.SetRootQueueDisc ("ns3::RedQueueDisc", "MaxSize", StringValue (ToString(BUFFER_SIZE)+"p"));

    NS_LOG_INFO ("Configuring servers");
    // Setting servers
    p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (LEAF_SERVER_CAPACITY)));
    p2p.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
    p2p.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));  // set basic queues to 1 packet

    ipv4.SetBase ("10.1.0.0", "255.255.255.0");

    std::vector<Ipv4Address> leafNetworks (LEAF_COUNT);

    for (int i = 0; i < LEAF_COUNT; i++)
    {
        Ipv4Address network = ipv4.NewNetwork ();
        leafNetworks[i] = network;

        for (int j = 0; j < SERVER_COUNT; j++)
        {
            int serverIndex = i * SERVER_COUNT + j;
            
            NS_LOG_INFO ("Server " << serverIndex << " is connected to leaf switch " << i);
            NodeContainer nodeContainer = NodeContainer (leaves.Get (i), servers.Get (serverIndex));
            NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);
		    
            // NS_LOG_INFO ("Install RED Queue for leaf: " << i << " and server: " << j);
	        // tch.Install (netDeviceContainer);
            
            serv2leafInterfaceContainer = ipv4.Assign (netDeviceContainer);
            ipv4.NewNetwork ();
        }
    }

    NS_LOG_INFO ("Configuring switches");
    // Setting switches
    p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (SPINE_LEAF_CAPACITY)));

    for (int i = 0; i < LEAF_COUNT; i++)
    {
	 	ipv4.NewNetwork ();
        for (int j = 0; j < SPINE_COUNT; j++)
        {
            uint64_t leafSpineCapacity = SPINE_LEAF_CAPACITY;
            if (j == 0)
            {
                leafSpineCapacity = SPINE_LEAF_CAPACITY / 2;
            }
            NodeContainer nodeContainer = NodeContainer (leaves.Get (i), spines.Get (j));
            p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (leafSpineCapacity)));
            NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);

            NS_LOG_INFO ("Install RED Queue for leaf: " << i << " and spine: " << j);
            tch.Install (netDeviceContainer);

 	        leaf2spineInterfaceContainer = ipv4.Assign (netDeviceContainer);
            // ipv4.NewNetwork ();


            NS_LOG_INFO ("Leaf " << i << " is connected to Spine " << j
                    << "(" << netDeviceContainer.Get (0)->GetIfIndex ()
                    << "<->" << netDeviceContainer.Get(1)->GetIfIndex () << ")"
                    << "at capacity: " << leafSpineCapacity);
	    }
    }

    ///////////Not  sure it's necessarry//////////////
    // and setup ip routing tables to get total ip-level connectivity.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    /////////////////////////////////////////////////////////

    NS_LOG_INFO ("Create Sockets, Applications and Sinks");

    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT));
    // Create a packet sink to receive these packets on n2
      // client type dependant parameters:
  if (transportProt.compare ("TCP") == 0)
    {
      socketType = "ns3::TcpSocketFactory";
    }
  else
    {
      socketType = "ns3::UdpSocketFactory";
    }
    PacketSinkHelper sink (socketType, sinkLocalAddress); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
    ApplicationContainer sinkApps, sourceApps;
    
    for (size_t i = 0; i < LEAF_COUNT; i++)
    {
        for (size_t j = 0; j < SERVER_COUNT; j++)
        {
            // create sockets
            ns3::Ptr<ns3::Socket> sockptr;

            if (transportProt.compare("TCP") == 0) 
            {
            // setup source socket
            sockptr =
                ns3::Socket::CreateSocket(servers.Get(j),
                        ns3::TcpSocketFactory::GetTypeId());
            ns3::Ptr<ns3::TcpSocket> tcpsockptr =
                ns3::DynamicCast<ns3::TcpSocket> (sockptr);
            } 
            else if (transportProt.compare("UDP") == 0) 
            {
            // setup source socket
            sockptr =
                ns3::Socket::CreateSocket(servers.Get(j),
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

            // Ptr<Node> destServer = servers.Get (i);
            // Ptr<Ipv4> ipv4Object = destServer->GetObject<Ipv4> ();
            // Ipv4InterfaceAddress destInterface = ipv4Object->GetAddress (1, 0);
            // Ipv4Address destAddress = destInterface.GetLocal ();

            // create and install Client apps:    
            if (applicationType.compare("standardClient") == 0) 
            {
            // Install UDP application on the sender 
            // send packet flows from servers with even indexes to spine 0, and from servers with odd indexes to spine 1. 
            UdpClientHelper clientHelper (leaf2spineInterfaceContainer.GetAddress(j % 2), SERV_PORT);
            clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
            clientHelper.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
            sourceApps.Add(clientHelper.Install (servers.Get(i)));
            sourceApps.Start (Seconds (1.0));
            sourceApps.Stop (Seconds(3.0));
            } 
            else if (applicationType.compare("OnOff") == 0) 
            {
            // Create the OnOff applications to send TCP/UDP to the server
            InetSocketAddress socketAddressUp = InetSocketAddress (leaf2spineInterfaceContainer.GetAddress(j % 2), SERV_PORT);
            OnOffHelper clientHelper (socketType, Address ());
            clientHelper.SetAttribute ("Remote", AddressValue (socketAddressUp));
            clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
            clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
            clientHelper.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
            clientHelper.SetAttribute ("DataRate", StringValue ("2Mb/s"));
            sourceApps.Add(clientHelper.Install (servers.Get(i)));
            sourceApps.Start (Seconds (1.0));
            sourceApps.Stop (Seconds(3.0));
            } 
            else if (applicationType.compare("customOnOff") == 0) 
            {
            // Create the Custom application to send TCP/UDP to the server
            
            InetSocketAddress socketAddressUp = InetSocketAddress (leaf2spineInterfaceContainer.GetAddress(j % 2), SERV_PORT);
            Ptr<CustomOnOffApplication> customOnOffApp = CreateObject<CustomOnOffApplication> ();
            customOnOffApp->Setup(sockptr);
            customOnOffApp->SetAttribute("Remote", AddressValue (socketAddressUp));
            customOnOffApp->SetAttribute("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
            customOnOffApp->SetAttribute("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
            customOnOffApp->SetAttribute("PacketSize", UintegerValue (PACKET_SIZE));
            customOnOffApp->SetAttribute("DataRate", StringValue ("2Mb/s"));
            customOnOffApp->SetAttribute("EnableSeqTsSizeHeader", BooleanValue (false));
            servers.Get(i)->AddApplication(customOnOffApp);
            customOnOffApp->SetStartTime (Seconds (1.0));
            customOnOffApp->SetStopTime (Seconds(3.0));
            }
            else if (applicationType.compare("customApplication") == 0)
            {
            // Create the Custom application to send TCP/UDP to the server
            uint32_t numOfPackets = 100;  // number of packets to send in one stream for custom application
            Ptr<TutorialApp> customApp = CreateObject<TutorialApp> ();
            InetSocketAddress socketAddressUp = InetSocketAddress (leaf2spineInterfaceContainer.GetAddress(j % 2), SERV_PORT);  // sink IpV4 Address
            customApp->Setup (sockptr, socketAddressUp, PACKET_SIZE, numOfPackets, DataRate ("1Mbps"));
            servers.Get(i)->AddApplication (customApp);
            customApp->SetStartTime (Seconds (1.0));
            customApp->SetStopTime (Seconds(3.0));
            } 
            else 
            {
            std::cerr << "unknown app type: " << applicationType << std::endl;
            exit(1);
            }
        }
        // setup sink
        sinkApps.Add(sink.Install (spines.Get(i)));
        sinkApps.Start (Seconds (START_TIME));
        sinkApps.Stop (Seconds (END_TIME + 0.1));
    }
     
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // BulkSendHelper sourceA ("ns3::TcpSocketFactory", InetSocketAddress (destAddress, FLOW_A_PORT));
    // sourceA.SetAttribute ("SendSize", UintegerValue (PACKET_SIZE));
    // sourceA.SetAttribute ("MaxBytes", UintegerValue(0));

    // ApplicationContainer sourceAppA = sourceA.Install (servers.Get (0));
    // sourceAppA.Start (Seconds (START_TIME));
    // sourceAppA.Stop (Seconds (END_TIME));

    // PacketSinkHelper sinkA ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), FLOW_A_PORT));
    // ApplicationContainer sinkAppA = sinkA.Install (servers.Get (1));
    // sinkAppA.Start (Seconds (START_TIME));
    // sinkAppA.Stop (Seconds (END_TIME));
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


    NS_LOG_INFO ("Enabling flow monitor");
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    NS_LOG_INFO ("Enabling link monitor");

    // Ptr<LinkMonitor> linkMonitor = Create<LinkMonitor> ();
    // for (int i = 0; i < SPINE_COUNT; i++)
    // {
    //     std::stringstream name;
    //     name << "Spine " << i;
    //     Ptr<Ipv4LinkProbe> spineLinkProbe = Create<Ipv4LinkProbe> (spines.Get (i), linkMonitor);
    //     spineLinkProbe->SetProbeName (name.str ());
    //     spineLinkProbe->SetCheckTime (Seconds (0.001));
    //     spineLinkProbe->SetDataRateAll (DataRate (SPINE_LEAF_CAPACITY));
    // }
    // for (int i = 0; i < LEAF_COUNT; i++)
    // {
    //     std::stringstream name;
    //     name << "Leaf " << i;
    //     Ptr<Ipv4LinkProbe> leafLinkProbe = Create<Ipv4LinkProbe> (leaves.Get (i), linkMonitor);
    //     leafLinkProbe->SetProbeName (name.str ());
    //     leafLinkProbe->SetCheckTime (Seconds (0.001));
    //     leafLinkProbe->SetDataRateAll (DataRate (SPINE_LEAF_CAPACITY));
    // }

    // linkMonitor->Start (Seconds (START_TIME));
    // linkMonitor->Stop (Seconds (END_TIME));

    NS_LOG_INFO ("Enable Throughput Tracing");

    remove (StringCombine ("flow_A_", ToString (flowletTimeout), "_throughput.plt").c_str ());
    remove (StringCombine ("flow_B_", ToString (flowletTimeout), "_throughput.plt").c_str ());

    throughputDatasetA.SetTitle ("Throughput A");
    throughputDatasetA.SetStyle (Gnuplot2dDataset::LINES_POINTS);
    throughputDatasetB.SetTitle ("Throughput B");
    throughputDatasetB.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    Simulator::ScheduleNow (&CheckFlowAThroughput, sinkApps.Get (0)->GetObject<PacketSink> ());
    Simulator::ScheduleNow (&CheckFlowBThroughput, sinkApps.Get (1)->GetObject<PacketSink> ());

    NS_LOG_INFO ("Start simulation");
    Simulator::Stop (Seconds (END_TIME));
    Simulator::Run ();

    flowMonitor->CheckForLostPackets ();

    std::stringstream flowMonitorFilename;
    std::stringstream linkMonitorFilename;

    flowMonitorFilename << "spine-leaf" << flowletTimeout << "-test-flow-monitor.xml";
    linkMonitorFilename << "spine-leaf" << flowletTimeout << "-test-link-monitor.out";

    flowMonitor->SerializeToXmlFile(flowMonitorFilename.str (), true, true);
    // linkMonitor->OutputToFile (linkMonitorFilename.str (), &DefaultFormat);

    Simulator::Destroy ();
    NS_LOG_INFO ("Stop simulation");

    DoGnuPlot (flowletTimeout);

    return 0;
}

