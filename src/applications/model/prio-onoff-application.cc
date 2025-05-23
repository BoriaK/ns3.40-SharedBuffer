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

#include "prio-onoff-application.h"

#include "ns3/address.h"
#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/packet-socket-address.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/string.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/core-module.h"

#include <iostream>
#include <fstream>
#include <string>

#include <stdint.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("PrioOnOffApplication");

NS_OBJECT_ENSURE_REGISTERED(PrioOnOffApplication);

TypeId
PrioOnOffApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::PrioOnOffApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<PrioOnOffApplication>()
            .AddAttribute("DataRate",
                          "The data rate in on state.",
                          DataRateValue(DataRate("500kb/s")),
                          MakeDataRateAccessor(&PrioOnOffApplication::m_cbrRate),
                          MakeDataRateChecker())
            .AddAttribute("PacketSize",
                          "The size of packets sent in on state",
                          UintegerValue(512),
                          MakeUintegerAccessor(&PrioOnOffApplication::m_pktSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Remote",
                          "The address of the destination",
                          AddressValue(),
                          MakeAddressAccessor(&PrioOnOffApplication::m_peer),
                          MakeAddressChecker())
            .AddAttribute("Local",
                          "The Address on which to bind the socket. If not set, it is generated "
                          "automatically.",
                          AddressValue(),
                          MakeAddressAccessor(&PrioOnOffApplication::m_local),
                          MakeAddressChecker())
            .AddAttribute("ApplicationToS",
                            "The Type of Service for all packets sent by this application. ",
                            UintegerValue(0x0),
                            MakeUintegerAccessor(&PrioOnOffApplication::m_appTos),
                            MakeUintegerChecker<uint32_t>())
            .AddAttribute("OnTime",
                          "A RandomVariableStream used to pick the duration of the 'On' state.",
                          StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                          MakePointerAccessor(&PrioOnOffApplication::m_onTime),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("OffTime",
                          "A RandomVariableStream used to pick the duration of the 'Off' state.",
                          StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                          MakePointerAccessor(&PrioOnOffApplication::m_offTime),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("MaxBytes",
                          "The total number of bytes to send. Once these bytes are sent, "
                          "no packet is sent again, even in on state. The value zero means "
                          "that there is no limit.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&PrioOnOffApplication::m_maxBytes),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("Protocol",
                          "The type of protocol to use. This should be "
                          "a subclass of ns3::SocketFactory",
                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                          MakeTypeIdAccessor(&PrioOnOffApplication::m_tid),
                          // This should check for SocketFactory as a parent
                          MakeTypeIdChecker())
            .AddAttribute("EnableSeqTsSizeHeader",
                          "Enable use of SeqTsSizeHeader for sequence number and timestamp",
                          BooleanValue(false),
                          MakeBooleanAccessor(&PrioOnOffApplication::m_enableSeqTsSizeHeader),
                          MakeBooleanChecker())
            .AddAttribute ("NumOfPacketsHighPrioThreshold", 
                            "The number of packets in a single sequence, up to which, "
                            "the flow will be tagged as High Priority. "
                            "If a generated flow is longer than this threshold, packets from it will be labaled as low priority",
                            UintegerValue (10),
                            MakeUintegerAccessor (&PrioOnOffApplication::m_threshold),
                            MakeUintegerChecker<uint8_t> ())
            .AddAttribute ("FlowPriority", 
                            "The priority assigned to all the packets created by this Application, "
                            "1 = high priority, 2 = low priority"
                            "if this attribute is added, it overrides other methods to define priority",
                            UintegerValue (0),
                            MakeUintegerAccessor (&PrioOnOffApplication::m_userSetPriority),
                            MakeUintegerChecker<uint32_t> ())
            .AddAttribute ("MiceElephantProbability", 
                            "The parameter D that's configured by the user. High D -> high mice probability, Low D -> high elephant probability"
                            "if this attribute is added, the aplication will add a MiceElephantProbTag with the assigned D value",
                            StringValue(""),
                            MakeStringAccessor(&PrioOnOffApplication::m_miceElephantProb),
                            MakeStringChecker())
            .AddAttribute ("StreamIndex", 
                            "Assign a stream index for each instanse of the OnOff Application for the RNG",
                            UintegerValue (1),
                            MakeUintegerAccessor (&PrioOnOffApplication::m_streamIndex),
                            MakeUintegerChecker<uint32_t> ())
            .AddTraceSource("Tx",
                            "A new packet is created and is sent",
                            MakeTraceSourceAccessor(&PrioOnOffApplication::m_txTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("TxWithAddresses",
                            "A new packet is created and is sent",
                            MakeTraceSourceAccessor(&PrioOnOffApplication::m_txTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback")
            .AddTraceSource("TxWithSeqTsSize",
                            "A new packet is created with SeqTsSizeHeader",
                            MakeTraceSourceAccessor(&PrioOnOffApplication::m_txTraceWithSeqTsSize),
                            "ns3::PacketSink::SeqTsSizeCallback");
    return tid;
}

PrioOnOffApplication::PrioOnOffApplication()
    : m_socket(nullptr),
      m_connected(false),
      m_residualBits(0),
      m_lastStartTime(Seconds(0)),
      m_totBytes(0),
      m_unsentPacket(nullptr),
      m_packetSeqCount(1), // number of sent packets per sequence, always start with 1, added by me!
    // m_priority(1),  // the priority of the generated flow.
    // m_threshold (10),  // Flow classfication Threshold (length), Added by me!
      m_isOn(false)
      
{
    NS_LOG_FUNCTION(this);
}

PrioOnOffApplication::~PrioOnOffApplication()
{
    NS_LOG_FUNCTION(this);
}

double_t 
StringToDouble (std::string value)
{
  double valueAsDouble;
  valueAsDouble = std::stod(value);
//   double newvalueAsDouble;
//   int valueAsInt = valueAsDouble * 100;
//   valueAsInt = std::round(valueAsInt);
//   newvalueAsDouble = valueAsInt / 100.0;

  return valueAsDouble;
}

int32_t 
StringToDecInt (std::string value)
{
  int32_t valueAsDecInt;
  valueAsDecInt = (std::stod(value)) * 10;
//   double newvalueAsDouble;
//   int valueAsInt = valueAsDouble * 100;
//   valueAsInt = std::round(valueAsInt);
//   newvalueAsDouble = valueAsInt / 100.0;

  return valueAsDecInt;
}

void
PrioOnOffApplication::SetMaxBytes(uint64_t maxBytes)
{
    NS_LOG_FUNCTION(this << maxBytes);
    m_maxBytes = maxBytes;
}

Ptr<Socket>
PrioOnOffApplication::GetSocket() const
{
    NS_LOG_FUNCTION(this);
    return m_socket;
}

bool
PrioOnOffApplication::GetCurrentState() const
{
    NS_LOG_FUNCTION(this);
    return m_isOn;
}

int64_t
PrioOnOffApplication::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_onTime->SetStream(stream);
    m_offTime->SetStream(stream + 1);
    // std::cout << "assigned stream for RNG is: " << m_onTime->GetStream() << std::endl;
    // std::cout << "assigned stream for RNG is: " << m_offTime->GetStream() << std::endl;
    return 2;
}

void
PrioOnOffApplication::DoDispose()
{
    NS_LOG_FUNCTION(this);

    CancelEvents();
    m_socket = nullptr;
    m_unsentPacket = nullptr;
    // chain up
    Application::DoDispose();
}

// Application Methods
void
PrioOnOffApplication::StartApplication() // Called at time specified by Start
{
    NS_LOG_FUNCTION(this);
    // RngSeedManager::SetSeed(1);
    // RngSeedManager::SetRun(1);

    // std::cout << "the RNG seed is: " << RngSeedManager::GetSeed() << std::endl;
    // std::cout << "the run number is: " << RngSeedManager::GetRun() << std::endl;

    AssignStreams(m_streamIndex);

    // Create the socket if not already
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), m_tid);
        int ret = -1;

        // Set the ToS on the application level
        m_socket->SetIpTos(m_appTos);

        if (!m_local.IsInvalid())
        {
            NS_ABORT_MSG_IF((Inet6SocketAddress::IsMatchingType(m_peer) &&
                             InetSocketAddress::IsMatchingType(m_local)) ||
                                (InetSocketAddress::IsMatchingType(m_peer) &&
                                 Inet6SocketAddress::IsMatchingType(m_local)),
                            "Incompatible peer and local address IP version");
            ret = m_socket->Bind(m_local);
        }
        else
        {
            if (Inet6SocketAddress::IsMatchingType(m_peer))
            {
                ret = m_socket->Bind6();
            }
            else if (InetSocketAddress::IsMatchingType(m_peer) ||
                     PacketSocketAddress::IsMatchingType(m_peer))
            {
                ret = m_socket->Bind();
            }
        }

        if (ret == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }

        m_socket->SetConnectCallback(MakeCallback(&PrioOnOffApplication::ConnectionSucceeded, this),
                                     MakeCallback(&PrioOnOffApplication::ConnectionFailed, this));

        m_socket->Connect(m_peer);
        m_socket->SetAllowBroadcast(true);
        m_socket->ShutdownRecv();
    }
    m_cbrRateFailSafe = m_cbrRate;

    // Ensure no pending event
    CancelEvents();

    // If we are not yet connected, there is nothing to do here,
    // the ConnectionComplete upcall will start timers at that time.
    // If we are already connected, CancelEvents did remove the events,
    // so we have to start them again.
    if (m_connected)
    {
        ScheduleStartEvent();
    }
}

void
PrioOnOffApplication::StopApplication() // Called at time specified by Stop
{
    NS_LOG_FUNCTION(this);

    CancelEvents();
    if (m_socket)
    {
        m_socket->Close();
    }
    else
    {
        NS_LOG_WARN("PrioOnOffApplication found null socket to close in StopApplication");
    }
}

void
PrioOnOffApplication::CancelEvents()
{
    NS_LOG_FUNCTION(this);

    if (m_sendEvent.IsRunning() && m_cbrRateFailSafe == m_cbrRate)
    { // Cancel the pending send packet event
        // Calculate residual bits since last packet sent
        Time delta(Simulator::Now() - m_lastStartTime);
        int64x64_t bits = delta.To(Time::S) * m_cbrRate.GetBitRate();
        m_residualBits += bits.GetHigh();
    }
    m_cbrRateFailSafe = m_cbrRate;
    Simulator::Cancel(m_sendEvent);
    Simulator::Cancel(m_startStopEvent);
    // Canceling events may cause discontinuity in sequence number if the
    // SeqTsSizeHeader is header, and m_unsentPacket is true
    if (m_unsentPacket)
    {
        NS_LOG_DEBUG("Discarding cached packet upon CancelEvents ()");
    }
    m_unsentPacket = nullptr;
}

// Event handlers
void
PrioOnOffApplication::StartSending()
{
    NS_LOG_FUNCTION(this);
    
    m_isOn = true; // Set the state to "on"
    std::cout << "State: ON" << std::endl; // Log the state
    m_lastStartTime = Simulator::Now();
    
    ScheduleNextTx(); // Schedule the send packet event
    ScheduleStopEvent();
}

void
PrioOnOffApplication::StopSending()
{
    NS_LOG_FUNCTION(this);

    m_isOn = false; // Set the state to "off"
    std::cout << "State: OFF" << std::endl; // Log the state
    
    CancelEvents();

    ScheduleStartEvent();
}

// Private helpers
void
PrioOnOffApplication::ScheduleNextTx()
{
    NS_LOG_FUNCTION(this);

    if (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    {
        NS_ABORT_MSG_IF(m_residualBits > m_pktSize * 8,
                        "Calculation to compute next send time will overflow");
        uint32_t bits = m_pktSize * 8 - m_residualBits;
        NS_LOG_LOGIC("bits = " << bits);
        Time nextTime(
            Seconds(bits / static_cast<double>(m_cbrRate.GetBitRate()))); // Time till next packet
        NS_LOG_LOGIC("nextTime = " << nextTime.As(Time::S));
        m_sendEvent = Simulator::Schedule(nextTime, &PrioOnOffApplication::SendPacket, this);
    }
    else
    { // All done, cancel any pending events
        StopApplication();
    }
}

void
PrioOnOffApplication::ScheduleStartEvent()
{ // Schedules the event to start sending data (switch to the "On" state)
    NS_LOG_FUNCTION(this);

    Time offInterval = Seconds(m_offTime->GetValue());
    // m_offTime->SetStream(1);
    // std::cout << "stream = " << m_offTime->GetStream() << std::endl;
    // std::cout << "off time = " << offInterval << "[sec]" << std::endl;
    NS_LOG_LOGIC("start at " << offInterval.As(Time::S));
    m_startStopEvent = Simulator::Schedule(offInterval, &PrioOnOffApplication::StartSending, this);
}

void
PrioOnOffApplication::ScheduleStopEvent()
{ // Schedules the event to stop sending data (switch to "Off" state)
    NS_LOG_FUNCTION(this);

    Time onInterval = Seconds(m_onTime->GetValue());
    // m_offTime->SetStream(1);
    // std::cout << "stream = " << m_onTime->GetStream() << std::endl;
    // std::cout << "on time = " << onInterval << "[sec]" << std::endl;
    NS_LOG_LOGIC("stop at " << onInterval.As(Time::S));
    m_startStopEvent = Simulator::Schedule(onInterval, &PrioOnOffApplication::StopSending, this);
    // std::cout << "flow length: " << m_packetSeqCount << std::endl;
    m_packetSeqCount = 1;  // not sure it's nesessary
}

void
PrioOnOffApplication::SendPacket()
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT(m_sendEvent.IsExpired());

    Ptr<Packet> packet;
    if (m_unsentPacket)
    {
        packet = m_unsentPacket;
    }
    else if (m_enableSeqTsSizeHeader)
    {
        Address from;
        Address to;
        m_socket->GetSockName(from);
        m_socket->GetPeerName(to);
        SeqTsSizeHeader header;
        header.SetSeq(m_seq++);
        header.SetSize(m_pktSize);
        NS_ABORT_IF(m_pktSize < header.GetSerializedSize());
        packet = Create<Packet>(m_pktSize - header.GetSerializedSize());
        // Trace before adding header, for consistency with PacketSink
        m_txTraceWithSeqTsSize(packet, from, to, header);
        packet->AddHeader(header);
    }
    else
    {
        packet = Create<Packet>(m_pktSize);
    }

    //// if D was assigned by the user:
    if (!m_miceElephantProb.empty())
    {
        // double miceElephantProbValue = StringToDouble(m_miceElephantProb);
        int32_t miceElephantProbValueDecInt = StringToDecInt(m_miceElephantProb); // a Tag has to be an Integer!
        
        miceElephantProbTag.SetSimpleValue(miceElephantProbValueDecInt);
        // store the tag in a packet.
        if (packet->PeekPacketTag(miceElephantProbTag) == false)
        {
            packet->AddPacketTag(miceElephantProbTag);
        }
        else
        {
            std::cout << "retransmit" << std::endl;
        }
        // packet->AddPacketTag (miceElephantProbTag);
    }

    SocketIpTosTag ipTosTag;  // used in the switch loop, and in the end of the function
    int setPriorityMode = 1;
    switch (setPriorityMode)
    {
        ////////////////// option 1: set the priority tag arbitrarly from a user requested param
        case 1:
        {
            m_priority = m_userSetPriority;
            break;
        }
        ////////////////// option 2: add priority tag to packet
        // create a tag.
        // set Tag value to depend on the number of previously sent packets
        // if m_packetSeqCount < m_threshold: TagValue->0x1 (High Priority)
        // if m_packetSeqCount >= m_threshold: TagValue->0x2 (Low Priority)
        case 2:
        {
            if (m_packetSeqCount < m_threshold)
            {
            m_priority = 0x1;
            // flowPrioTag.SetSimpleValue (0x1);
            }
            else 
            m_priority = 0x2;
            // flowPrioTag.SetSimpleValue (0x2);
            break;
        }
        /////////////////////// option 3: add a priority to a packet based on ToS value.
        // Tos 0x0 -> Low Priority -> TagValue->0x2 (Low Priority)
        // ToS 0x10 -> High Priority -> TagValue->0x1 (High Priority)
        case 3:
        {
            if (packet->PeekPacketTag(ipTosTag))
            {
                if (ipTosTag.GetTos() == 0x10)
                {
                    m_priority = 0x1;
                }
                else if (ipTosTag.GetTos() == 0x00 || ipTosTag.GetTos() == 0x02 || ipTosTag.GetTos() == 0x04 || ipTosTag.GetTos() == 0x06 || ipTosTag.GetTos() == 0x08)
                {
                    m_priority = 0x2;
                }
            }
            break;
        }
        default:
            break;
    }

    flowPrioTag.SetSimpleValue (m_priority);
    // store the tag in a packet.
    if (packet->PeekPacketTag(flowPrioTag) == false)
    {
        packet->AddPacketTag (flowPrioTag);
    }
    else
    {
        std::cout << "retransmit" << std::endl;
    }
    
    // packet->AddPacketTag (flowPrioTag);

    //debug:
    packet->PeekPacketTag(ipTosTag);
    std::cout << "packet: " << packet << " ToS from tag : " << int(ipTosTag.GetTos()) << std::endl;
    std::cout << "socket: " << m_socket << std::endl;
    std::cout << "packet: " << packet << " ToS from socket : " << int(m_socket->GetIpTos()) << std::endl;
    /////////////
    if (m_tid == ns3::TcpSocketFactory::GetTypeId())
    {
        // ipTosTag.SetTos(m_socket->GetIpTos()); // set Tos value from socket
        ipTosTag.SetTos(m_appTos); // directly set the ToS value from Application
        std::cout << "packet: " << packet << " ToS from tag : " << int(ipTosTag.GetTos()) << std::endl;
        appTosTag.SetTosValue(m_appTos); // force set the ToS value from Application
        std::cout << "packet: " << packet << " ToS from force tag : " << int(appTosTag.GetTosValue()) << std::endl;
    }
    else if (m_tid == ns3::UdpSocketFactory::GetTypeId())
    {
        std::cout << "Application port: " << "" << " is Type : UDP" << std::endl;
    }
    

    int actual = m_socket->Send(packet);

    if ((unsigned)actual == m_pktSize)
    {
        m_txTrace(packet);
        m_totBytes += m_pktSize;
        m_unsentPacket = nullptr;
        m_packetSeqCount++;
        Address localAddress;
        m_socket->GetSockName(localAddress);
        if (InetSocketAddress::IsMatchingType(m_peer))
        {
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " on-off application sent "
                                   << packet->GetSize() << " bytes to "
                                   << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() << " port "
                                   << InetSocketAddress::ConvertFrom(m_peer).GetPort()
                                   << " total Tx " << m_totBytes << " bytes");
            m_txTraceWithAddresses(packet, localAddress, InetSocketAddress::ConvertFrom(m_peer));
        }
        else if (Inet6SocketAddress::IsMatchingType(m_peer))
        {
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " on-off application sent "
                                   << packet->GetSize() << " bytes to "
                                   << Inet6SocketAddress::ConvertFrom(m_peer).GetIpv6() << " port "
                                   << Inet6SocketAddress::ConvertFrom(m_peer).GetPort()
                                   << " total Tx " << m_totBytes << " bytes");
            m_txTraceWithAddresses(packet, localAddress, Inet6SocketAddress::ConvertFrom(m_peer));
        }
    }
    else
    {
        NS_LOG_DEBUG("Unable to send packet; actual " << actual << " size " << m_pktSize
                                                      << "; caching for later attempt");
        m_unsentPacket = packet;
    }
    m_residualBits = 0;
    packet->PeekPacketTag(ipTosTag);
    
    // for debug:
    std::cout << "packet: " << packet << " ToS : " << int(ipTosTag.GetTos()) << std::endl;
    ///////////////////////
    
    m_lastStartTime = Simulator::Now();
    ScheduleNextTx();
}

void
PrioOnOffApplication::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    ScheduleStartEvent();
    m_connected = true;
}

void
PrioOnOffApplication::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_FATAL_ERROR("Can't connect");
}

} // Namespace ns3
