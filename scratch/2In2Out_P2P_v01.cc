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
 *            t0    r0 
 *             \   /  
 *              \ /
 *               S  
 *              / \  
 *             /   \ 
 *            t1    r1
 * 
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
#include "ns3/names.h"


// There are 2 servers connecting to a leaf switch
#define SERVER_COUNT 2
#define SWITCH_COUNT 1
#define RECIEVER_COUNT 2

#define SWITCH_RECIEVER_CAPACITY  2000000              // Total Leaf-Spine Capacity 2Mbps
#define SERVER_SWITCH_CAPACITY 20000000         // Total Serever-Leaf Capacity 20Mbps
#define LINK_LATENCY MicroSeconds(20)             // each link latency 10 MicroSeconds 
#define BUFFER_SIZE 100                           // Buffer Size (for each queue) 100 Packets

// The simulation starting and ending time
#define START_TIME 0.0
#define END_TIME 30

// The flow port range, each flow will be assigned a random port number within this range
#define SERV_PORT 50000

// Adopted from the simulation from snowzjx
// Acknowledged to https://github.com/snowzjx/ns3-load-balance.git
#define PACKET_SIZE 1024 // 1024 [bytes]

using namespace ns3;

std::string dir = "./Trace_Plots/basic_2In2Out_Topology/";
std::string traffic_control_type = "FifoQueueDisc"; // "FifoQueueDisc"/"RedQueueDisc"/"DT_FifoQueueDisc_v02"/"FB_FifoQueueDisc_v01"

uint32_t prev = 0;
Time prevTime = Seconds (0);

NS_LOG_COMPONENT_DEFINE ("basic_2In2Out");

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
TrafficControllPacketsInQueue_Trace (std::size_t index, uint32_t oldValue, uint32_t newValue)
{
  std::cout << "TrafficControllPacketsInQueue " << index << ": " << newValue << std::endl;
}

void
DevicePacketsInQueueTrace (Ptr<NetDevice> ndev, uint32_t oldValue, uint32_t newValue)
{
  std::cout << "PacketsInQueueOnNetDevice " << ndev << ": " << newValue << std::endl;
}

void
SojournTimeTrace (Time sojournTime)
{
  std::cout << "Sojourn time " << sojournTime.ToDouble (Time::MS) << "ms" << std::endl;
}

int main (int argc, char *argv[])
{
    LogComponentEnable ("basic_2In2Out", LOG_LEVEL_INFO);

    std::string applicationType = "customOnOff"; // "standardClient"/"OnOff"/"customApplication"/"customOnOff"
    // Command line parameters parsing
    std::string transportProt = "UDP"; // "UDP"/"TCP"
    std::string socketType;
    std::string queue_capacity;
    uint32_t alpha_high = 2;
    uint32_t alpha_low = 1;
    uint32_t flowletTimeout = 50;
    bool eraseOldData = true; // true/false

    if (eraseOldData == true)
    {
        system (("rm " + dir + traffic_control_type + "/*.dat").c_str ()); // to erase the old .dat files and collect new data
        system (("rm " + dir + traffic_control_type + "/*.txt").c_str ()); // to erase the previous test run summary, and collect new data
        std::cout << std::endl << "***Erased Previous Data***\n" << std::endl;
    }

    CommandLine cmd;
    cmd.AddValue ("transportProt", "Transport protocol to use: Udp, Tcp, DcTcp", transportProt);
    cmd.AddValue ("flowletTimeout", "flowlet timeout", flowletTimeout);
    cmd.Parse (argc, argv);

    NS_LOG_INFO ("Config parameters");
    // Application type dependent parameters
    if (applicationType.compare("standardClient") == 0)
    {
      queue_capacity = "20p"; // B, the total space on the buffer.
    }
    else if (applicationType.compare("OnOff") == 0 || applicationType.compare("customOnOff") == 0 || applicationType.compare("customApplication") == 0)
    {
      queue_capacity = BUFFER_SIZE; // B, the total space on the buffer [packets]
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
    uint64_t serverSwitchCapacity = SERVER_SWITCH_CAPACITY / SERVER_COUNT;
    n2s.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (serverSwitchCapacity)));
    n2s.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
    n2s.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));  // set basic queues to 1 packet
    // setting routers
    uint64_t switchRecieverCapacity = SWITCH_RECIEVER_CAPACITY / RECIEVER_COUNT;
    s2r.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (switchRecieverCapacity)));
    s2r.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
    s2r.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue (ToString(BUFFER_SIZE) + "p"));

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


    // Now add ip/tcp stack to all nodes. this is a VERY IMPORTANT COMPONENT!!!!
    NS_LOG_INFO ("Install Internet stacks");
    InternetStackHelper internet;
    internet.InstallAll ();

    NS_LOG_INFO ("Install QueueDisc");
    TrafficControlHelper tch;
    tch.SetRootQueueDisc ("ns3::" + traffic_control_type, "MaxSize", StringValue (ToString(BUFFER_SIZE)+"p"));
    
    QueueDiscContainer qdiscs = tch.Install (switchDevicesOut);  // in this option we installed TCH on switchDevicesOut. to send data from switch to reciever
    // QueueDiscContainer qdiscs = tch.Install (switchDevicesIn); // in this option we installed TCH on switchDevicesIn. to send Tx Ack from reciever to server. TCP only

    //////////////monitor q-discs//////////////////////////////
    for (size_t i = 0; i < RECIEVER_COUNT; i++)
    {
        Ptr<QueueDisc> queue = qdiscs.Get (i); // look at the router queue
        queue->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&TrafficControllPacketsInQueue_Trace, i));
    }

    /////////////////////////////Monitor the NetDevice//////////////////////////////
    for (size_t i = 0; i < RECIEVER_COUNT; i++)
    {
        Ptr<NetDevice> swnd = switchDevicesOut.Get(i);
        Ptr<PointToPointNetDevice> swndev = DynamicCast<PointToPointNetDevice>(swnd);
        Ptr<Queue<Packet>> queue = swndev->GetQueue();
        queue->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&DevicePacketsInQueueTrace, swnd));
    }
/////////////////////////////////////////////////////////////////////////////////


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

    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), SERV_PORT));

    PacketSinkHelper sink (socketType, sinkLocalAddress); // socketType is: "ns3::TcpSocketFactory" or "ns3::UdpSocketFactory"
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

        // create and install Client apps:    
        if (applicationType.compare("standardClient") == 0) 
        {
        // Install UDP application on the sender 
        // send packet flows from servers with even indexes to spine 0, and from servers with odd indexes to spine 1. 
            UdpClientHelper clientHelper (recieverIFs.GetAddress(recieverIndex), SERV_PORT);
            clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
            clientHelper.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
            sourceApps.Add(clientHelper.Install (servers.Get(serverIndex)));
            sourceApps.Start (Seconds (1.0));
            sourceApps.Stop (Seconds(3.0));
        } 
        else if (applicationType.compare("OnOff") == 0) 
        {
        // Create the OnOff applications to send TCP/UDP to the server
            InetSocketAddress socketAddressUp = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT);
            OnOffHelper clientHelper (socketType, Address ());
            clientHelper.SetAttribute ("Remote", AddressValue (socketAddressUp));
            clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
            clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
            clientHelper.SetAttribute ("PacketSize", UintegerValue (PACKET_SIZE));
            clientHelper.SetAttribute ("DataRate", StringValue ("2Mb/s"));
            sourceApps.Add(clientHelper.Install (servers.Get(serverIndex)));
            sourceApps.Start (Seconds (1.0));
            sourceApps.Stop (Seconds(3.0));
        } 
        else if (applicationType.compare("customOnOff") == 0) 
        {
        // Create the Custom application to send TCP/UDP to the server
            InetSocketAddress socketAddressUp = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT);
            Ptr<CustomOnOffApplication> customOnOffApp = CreateObject<CustomOnOffApplication> ();
            customOnOffApp->Setup(sockptr);
            customOnOffApp->SetAttribute("Remote", AddressValue (socketAddressUp));
            customOnOffApp->SetAttribute("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
            customOnOffApp->SetAttribute("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
            customOnOffApp->SetAttribute("PacketSize", UintegerValue (PACKET_SIZE));
            customOnOffApp->SetAttribute("DataRate", StringValue ("2Mb/s"));
            customOnOffApp->SetAttribute("EnableSeqTsSizeHeader", BooleanValue (false));
            servers.Get(serverIndex)->AddApplication(customOnOffApp);
            customOnOffApp->SetStartTime (Seconds (1.0));
            customOnOffApp->SetStopTime (Seconds(3.0));
        }
        else if (applicationType.compare("customApplication") == 0)
        {
        // Create the Custom application to send TCP/UDP to the server
            uint32_t numOfPackets = 100;  // number of packets to send in one stream for custom application
            Ptr<TutorialApp> customApp = CreateObject<TutorialApp> ();
            InetSocketAddress socketAddressUp = InetSocketAddress (recieverIFs.GetAddress(recieverIndex), SERV_PORT);  // sink IpV4 Address
            customApp->Setup (sockptr, socketAddressUp, PACKET_SIZE, numOfPackets, DataRate ("1Mbps"));
            servers.Get(serverIndex)->AddApplication (customApp);
            customApp->SetStartTime (Seconds (1.0));
            customApp->SetStopTime (Seconds(3.0));
        } 
        else 
        {
            std::cerr << "unknown app type: " << applicationType << std::endl;
            exit(1);
        }
        
        // setup sink
        sinkApps.Add(sink.Install (recievers.Get(recieverIndex)));
        sinkApps.Start (Seconds (START_TIME));
        sinkApps.Stop (Seconds (END_TIME + 0.1));
    }

    NS_LOG_INFO ("Enabling flow monitor");
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Create a new directory to store the output of the program
    std::string dirToSave = "mkdir -p " + dir + traffic_control_type;
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

    std::cout << std::endl << "*** TC Layer statistics ***" << std::endl;
    // for (size_t i = 0; i < qdiscs.GetN(); i++)
    // {
    //     std::cout << "Queue Disceplene " << i << ":" << std::endl;
    //     std::cout << qdiscs.Get(i)->GetStats () << std::endl;
    // }
    // std::cout << tc->GetStats() << std::endl;

    
    // Added to create a .txt file with the summary of the tested scenario statistics
    std::ofstream testFlowStatistics (dir + traffic_control_type + "/Statistics.txt", std::ios::out | std::ios::app);
    testFlowStatistics << "Topology: 2In2Out" << std::endl;
    testFlowStatistics << "Queueing Algorithm: " + traffic_control_type << std::endl;
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
    for (size_t i = 0; i < qdiscs.GetN(); i++)
    {
        testFlowStatistics << "Queue Disceplene " << i << ":" << std::endl;
        testFlowStatistics << qdiscs.Get(i)->GetStats () << std::endl;
    }
    testFlowStatistics.close ();

    // command line needs to be in ./scratch/ inorder for the script to produce gnuplot correctly///
    // system (("gnuplot " + dir + "gnuplotScriptTcHighPriorityPacketsInQueue").c_str ());

    Simulator::Destroy ();
    NS_LOG_INFO ("Stop simulation");

    return 0;
}

