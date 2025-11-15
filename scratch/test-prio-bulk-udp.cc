/*
 * Test script to verify PrioBulkSendApplication works with UDP
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TestPrioBulkUdp");

int
main(int argc, char* argv[])
{
    bool useUdp = true;
    
    CommandLine cmd;
    cmd.AddValue("useUdp", "Use UDP instead of TCP", useUdp);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("PrioBulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create PrioBulkSend application
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    
    std::string protocol = useUdp ? "ns3::UdpSocketFactory" : "ns3::TcpSocketFactory";
    
    PrioBulkSendHelper source(protocol, sinkAddress);
    source.SetAttribute("SendSize", UintegerValue(1024));
    source.SetAttribute("MaxBytes", UintegerValue(10240)); // Send 10 KB
    source.SetAttribute("FlowPriority", UintegerValue(0x1)); // High priority
    source.SetAttribute("ApplicationToS", UintegerValue(0x10));
    
    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    // Create packet sink to receive the data
    PacketSinkHelper sink(protocol, InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(11.0));

    // Run simulation
    Simulator::Stop(Seconds(11.0));
    
    std::cout << "Running PrioBulkSend test with " << (useUdp ? "UDP" : "TCP") << std::endl;
    
    Simulator::Run();
    
    // Get statistics
    Ptr<PacketSink> sinkPtr = DynamicCast<PacketSink>(sinkApps.Get(0));
    std::cout << "Total Bytes Received: " << sinkPtr->GetTotalRx() << std::endl;
    
    Simulator::Destroy();
    
    return 0;
}
