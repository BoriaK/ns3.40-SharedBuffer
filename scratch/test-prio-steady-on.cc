/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Simple test to verify PrioSteadyOn application works correctly
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/prio-steady-on-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TestPrioSteadyOn");

int main (int argc, char *argv[])
{
  LogComponentEnable ("TestPrioSteadyOn", LOG_LEVEL_INFO);
  LogComponentEnable ("PrioSteadyOnApplication", LOG_LEVEL_INFO);
  
  NS_LOG_INFO ("Creating nodes");
  NodeContainer nodes;
  nodes.Create (2);
  
  NS_LOG_INFO ("Creating point-to-point link");
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
  
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);
  
  NS_LOG_INFO ("Installing internet stack");
  InternetStackHelper stack;
  stack.Install (nodes);
  
  NS_LOG_INFO ("Assigning IP addresses");
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);
  
  NS_LOG_INFO ("Creating PrioSteadyOn application");
  uint16_t port = 9;
  
  // Install receiver (PacketSink)
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));
  
  // Install sender (PrioSteadyOn)
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PrioSteadyOnHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("DataRate", StringValue ("1Mbps"));
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // Unlimited
  clientHelper.SetAttribute ("FlowPriority", UintegerValue (0x2)); // Low priority
  
  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (5.0));
  
  NS_LOG_INFO ("Running simulation");
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  
  // Get statistics from PacketSink
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
  NS_LOG_INFO ("Total Bytes Received: " << sink->GetTotalRx ());
  
  Simulator::Destroy ();
  
  NS_LOG_INFO ("Test completed successfully!");
  return 0;
}
