/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// Copyright (c) 2006 Georgia Tech Research Corporation
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Author: George F. Riley<riley@ece.gatech.edu>
//

// ns3 - On/Off Data Source Application class
// George F. Riley, Georgia Tech, Spring 2007
// Adapted from ApplicationOnOff in GTNetS.

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "custom_onoff-application.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/boolean.h"
// #include "ns3/custom-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("CustomOnOffApplication");

NS_OBJECT_ENSURE_REGISTERED (CustomOnOffApplication);

TypeId
CustomOnOffApplication::GetTypeId (void)
{
  // static TypeId tid = TypeId ("ns3::CustomOnOffApplication")
  static TypeId tid = TypeId ("CustomOnOffApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<CustomOnOffApplication> ()
    .AddAttribute ("DataRate", "The data rate in on state.",
                   DataRateValue (DataRate ("500kb/s")),
                   MakeDataRateAccessor (&CustomOnOffApplication::m_cbrRate),
                   MakeDataRateChecker ())
    .AddAttribute ("PacketSize", "The size of packets sent in on state",
                   UintegerValue (512),
                   MakeUintegerAccessor (&CustomOnOffApplication::m_pktSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&CustomOnOffApplication::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("Local",
                   "The Address on which to bind the socket. If not set, it is generated automatically.",
                   AddressValue (),
                   MakeAddressAccessor (&CustomOnOffApplication::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("OnTime", "A RandomVariableStream used to pick the duration of the 'On' state.",
                   StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                   MakePointerAccessor (&CustomOnOffApplication::m_onTime),
                   MakePointerChecker <RandomVariableStream>())
    .AddAttribute ("OffTime", "A RandomVariableStream used to pick the duration of the 'Off' state.",
                   StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                   MakePointerAccessor (&CustomOnOffApplication::m_offTime),
                   MakePointerChecker <RandomVariableStream>())
    .AddAttribute ("MaxBytes", 
                   "The total number of bytes to send. Once these bytes are sent, "
                   "no packet is sent again, even in on state. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&CustomOnOffApplication::m_maxBytes),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("Protocol", "The type of protocol to use. This should be "
                   "a subclass of ns3::SocketFactory",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&CustomOnOffApplication::m_tid),
                   // This should check for SocketFactory as a parent
                   MakeTypeIdChecker ())
    .AddAttribute ("EnableSeqTsSizeHeader",
                   "Enable use of SeqTsSizeHeader for sequence number and timestamp",
                   BooleanValue (false),
                   MakeBooleanAccessor (&CustomOnOffApplication::m_enableSeqTsSizeHeader),
                   MakeBooleanChecker ())
    .AddAttribute ("NumOfPacketsHighPrioThreshold", 
                   "The number of packets in a single sequence, up to which, "
                   "the flow will be tagged as High Priority. "
                   "If a generated flow is longer than this threshold, packets from it will be labaled as low priority",
                   UintegerValue (10),
                   MakeUintegerAccessor (&CustomOnOffApplication::m_threshold),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("FlowPriority", 
                   "The priority assigned to all the packets created by this Application, "
                   "1 = high priority, 2 = low priority"
                   "if this attribute is added, it overrides other methods to define priority",
                   UintegerValue (0),
                   MakeUintegerAccessor (&CustomOnOffApplication::m_userSetPriority),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&CustomOnOffApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("TxWithAddresses", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&CustomOnOffApplication::m_txTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
    .AddTraceSource ("TxWithSeqTsSize", "A new packet is created with SeqTsSizeHeader",
                     MakeTraceSourceAccessor (&CustomOnOffApplication::m_txTraceWithSeqTsSize),
                     "ns3::PacketSink::SeqTsSizeCallback")
  ;
  return tid;
}

CustomOnOffApplication::CustomOnOffApplication ()
  : m_socket (0),
    m_connected (false),
    m_residualBits (0),
    m_lastStartTime (Seconds (0)),
    m_totBytes (0),
    m_packetsSent (0), // total number of sent packets, added by me
    m_packetSeqCount(1), // number of sent packets per sequence, always start with 1, added by me!
    // m_priority(1),  // the priority of the generated flow.
    // m_threshold (10),  // Flow classfication Threshold (length), Added by me!
    m_unsentPacket (0),
    m_isOn(false)
{
  NS_LOG_FUNCTION (this);
}

CustomOnOffApplication::~CustomOnOffApplication()
{
  NS_LOG_FUNCTION (this);
}

void
CustomOnOffApplication::Setup (Ptr<Socket> socket)
{
  m_socket = socket;
  // m_peer = address;
  // m_pktSize = packetSize;
  // m_nPackets = nPackets;
  // m_cbrRate = dataRate;
  // m_onTime = onTime;
  // m_offTime = offTime;
}

void 
CustomOnOffApplication::SetMaxBytes (uint64_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

// Ptr<Socket>
// CustomOnOffApplication::GetSocket (void) const
// {
//   NS_LOG_FUNCTION (this);
//   return m_socket;
// }

bool
CustomOnOffApplication::GetCurrentState() const
{
    NS_LOG_FUNCTION(this);
    return m_isOn;
}

int64_t 
CustomOnOffApplication::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_onTime->SetStream (stream);
  m_offTime->SetStream (stream + 1);
  return 2;
}

void
CustomOnOffApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  CancelEvents ();
  m_socket = 0;
  m_unsentPacket = 0;
  // chain up
  Application::DoDispose ();
}

// Application Methods
void CustomOnOffApplication::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  m_socket->Connect (m_peer);
  m_socket->SetAllowBroadcast (true);
  m_socket->ShutdownRecv ();

  m_socket->SetConnectCallback (
    MakeCallback (&CustomOnOffApplication::ConnectionSucceeded, this),
    MakeCallback (&CustomOnOffApplication::ConnectionFailed, this));
  m_cbrRateFailSafe = m_cbrRate;

  // Insure no pending event
  CancelEvents ();
  // If we are not yet connected, there is nothing to do here
  // The ConnectionComplete upcall will start timers at that time
  //if (!m_connected) return;
  ScheduleStartEvent ();
}

void CustomOnOffApplication::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  CancelEvents ();
  // if(m_socket != 0)
  if(m_socket)
    {
      m_socket->Close ();
    }
  else
    {
      NS_LOG_WARN ("OnOffApplication found null socket to close in StopApplication");
    }
}

void CustomOnOffApplication::CancelEvents ()
{
  NS_LOG_FUNCTION (this);

  if (m_sendEvent.IsRunning () && m_cbrRateFailSafe == m_cbrRate )
    { // Cancel the pending send packet event
      // Calculate residual bits since last packet sent
      Time delta (Simulator::Now () - m_lastStartTime);
      int64x64_t bits = delta.To (Time::S) * m_cbrRate.GetBitRate ();
      m_residualBits += bits.GetHigh ();
    }
  m_cbrRateFailSafe = m_cbrRate;
  Simulator::Cancel (m_sendEvent);
  Simulator::Cancel (m_startStopEvent);
  // Canceling events may cause discontinuity in sequence number if the
  // SeqTsSizeHeader is header, and m_unsentPacket is true
  if (m_unsentPacket)
    {
      NS_LOG_DEBUG ("Discarding cached packet upon CancelEvents ()");
    }
  m_unsentPacket = 0;
}

// Event handlers
void CustomOnOffApplication::StartSending ()
{
  NS_LOG_FUNCTION (this);

  m_isOn = true; // Set the state to "on"
  // std::cout << "State: ON" << std::endl; // Log the state
  m_lastStartTime = Simulator::Now();

  ScheduleNextTx ();  // Schedule the send packet event
  ScheduleStopEvent ();
}

void CustomOnOffApplication::StopSending ()
{
  NS_LOG_FUNCTION(this);

  m_isOn = false; // Set the state to "off"
  // std::cout << "State: OFF" << std::endl; // Log the state
  
  CancelEvents();
  ScheduleStartEvent();
}

// Private helpers
void CustomOnOffApplication::ScheduleNextTx ()
{
  NS_LOG_FUNCTION (this);

  if (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    {
      NS_ABORT_MSG_IF (m_residualBits > m_pktSize * 8, "Calculation to compute next send time will overflow");
      uint32_t bits = m_pktSize * 8 - m_residualBits;
      NS_LOG_LOGIC ("bits = " << bits);
      Time nextTime (Seconds (bits /
                              static_cast<double>(m_cbrRate.GetBitRate ()))); // Time till next packet
      NS_LOG_LOGIC ("nextTime = " << nextTime.As (Time::S));
      m_sendEvent = Simulator::Schedule (nextTime,
                                         &CustomOnOffApplication::SendPacket, this);
    }
  else
    { // All done, cancel any pending events
      StopApplication ();
    }
}

void CustomOnOffApplication::ScheduleStartEvent ()
{  // Schedules the event to start sending data (switch to the "On" state)
  NS_LOG_FUNCTION (this);

  Time offInterval = Seconds (m_offTime->GetValue ());
  NS_LOG_LOGIC ("start at " << offInterval.As (Time::S));
  m_startStopEvent = Simulator::Schedule (offInterval, &CustomOnOffApplication::StartSending, this);
}

void CustomOnOffApplication::ScheduleStopEvent ()
{  // Schedules the event to stop sending data (switch to "Off" state)
  NS_LOG_FUNCTION (this);

  Time onInterval = Seconds (m_onTime->GetValue ());
  NS_LOG_LOGIC ("stop at " << onInterval.As (Time::S));
  m_startStopEvent = Simulator::Schedule (onInterval, &CustomOnOffApplication::StopSending, this);
  m_packetSeqCount = 1;
}


void CustomOnOffApplication::SendPacket ()
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_sendEvent.IsExpired ());

  Ptr<Packet> packet;
  if (m_unsentPacket)
    {
      packet = m_unsentPacket;
    }
  else if (m_enableSeqTsSizeHeader)
    {
      Address from, to;
      m_socket->GetSockName (from);
      m_socket->GetPeerName (to);
      SeqTsSizeHeader header;
      header.SetSeq (m_seq++);
      header.SetSize (m_pktSize);
      NS_ABORT_IF (m_pktSize < header.GetSerializedSize ());
      packet = Create<Packet> (m_pktSize - header.GetSerializedSize ());
      // Trace before adding header, for consistency with PacketSink
      m_txTraceWithSeqTsSize (packet, from, to, header);
      packet->AddHeader (header);
    }
  else
    {
      packet = Create<Packet> (m_pktSize);
    }
  
  ////////////////// option 1: set the priority tag arbitrarly from a user requested param
  
  /////////////////////// option 2: add priority tag to packet
  // create a tag.
  // set Tag value to depend on the number of previously sent packets
  // if m_packetSeqCount < m_threshold: TagValue->0x1 (High Priority)
  // if m_packetSeqCount >= m_threshold: TagValue->0x2 (Low Priority)
  if (m_userSetPriority)
  {
    m_priority = m_userSetPriority;
  }
  else
  {
    if (m_packetSeqCount < m_threshold)
    {
      m_priority = 0x1;
      // flowPrioTag.SetSimpleValue (0x1);
    }
    else 
      m_priority = 0x2;
      // flowPrioTag.SetSimpleValue (0x2);
  }

  flowPrioTag.SetSimpleValue (m_priority);
  // store the tag in a packet.
  packet->AddPacketTag (flowPrioTag);
/////////////////////////

// /////////////////////// option 3: add sequence conter tag to packet. Currently not used!
//   // create a tag.
//   MyTag flowPacketCounterTag;
// // add a Flow Packet Counter Tag to each packet in Tx
// // Rx can asign priority based on sequence length

//   flowPacketCounterTag.SetSimpleValue(m_packetSeqCount);
//   // store the tag in a packet.
//   packet->AddPacketTag (flowPacketCounterTag);
// ///////////////////////////

/////////////////////// option 4: add a priority to a packet based on ToS value.
// Tos 0x0 -> Low Priority -> TagValue->0x2 (Low Priority)
// ToS 0x6 -> High Priority -> TagValue->0x1 (High Priority)
////////////////////////////////////////////////////////////////////

  int actual = m_socket->Send (packet);
  if ((unsigned) actual == m_pktSize)
    {
      m_txTrace (packet);
      m_totBytes += m_pktSize;
      m_packetsSent++;
      m_packetSeqCount++;
      m_unsentPacket = 0;
      Address localAddress;
      m_socket->GetSockName (localAddress);
      if (InetSocketAddress::IsMatchingType (m_peer))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().As (Time::S)
                       << " on-off application sent "
                       <<  packet->GetSize () << " bytes to "
                       << InetSocketAddress::ConvertFrom(m_peer).GetIpv4 ()
                       << " port " << InetSocketAddress::ConvertFrom (m_peer).GetPort ()
                       << " total Tx " << m_totBytes << " bytes");
          m_txTraceWithAddresses (packet, localAddress, InetSocketAddress::ConvertFrom (m_peer));
        }
      else if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().As (Time::S)
                       << " on-off application sent "
                       <<  packet->GetSize () << " bytes to "
                       << Inet6SocketAddress::ConvertFrom(m_peer).GetIpv6 ()
                       << " port " << Inet6SocketAddress::ConvertFrom (m_peer).GetPort ()
                       << " total Tx " << m_totBytes << " bytes");
          m_txTraceWithAddresses (packet, localAddress, Inet6SocketAddress::ConvertFrom(m_peer));
        }
    }
  else
    {
      NS_LOG_DEBUG ("Unable to send packet; actual " << actual << " size " << m_pktSize << "; caching for later attempt");
      m_unsentPacket = packet;
    }
  m_residualBits = 0;
  m_lastStartTime = Simulator::Now ();
  ScheduleNextTx ();
}


void CustomOnOffApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  m_connected = true;
}

void CustomOnOffApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_FATAL_ERROR ("Can't connect");
}


} // Namespace ns3
