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
// Author: George F. Riley <riley@ece.gatech.edu>
// Modified to create PrioSteadyOn - continuous traffic without ON/OFF periods
//

#include "prio-steady-on-application.h"

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
#include "ns3/uinteger.h"
#include "ns3/tcp-socket-factory.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("PrioSteadyOnApplication");

NS_OBJECT_ENSURE_REGISTERED(PrioSteadyOnApplication);

// Helper function to convert string to double
static double
StringToDouble(std::string value)
{
  double valueAsDouble = std::stod(value);
  return valueAsDouble;
}

// Helper function to convert string to decimal integer (for tagging)
static int32_t
StringToDecInt(std::string value)
{
  int32_t valueAsDecInt;
  valueAsDecInt = (std::stod(value)) * 10;
  return valueAsDecInt;
}

TypeId
PrioSteadyOnApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::PrioSteadyOnApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<PrioSteadyOnApplication>()
            .AddAttribute("DataRate",
                          "The data rate in on state.",
                          DataRateValue(DataRate("500kb/s")),
                          MakeDataRateAccessor(&PrioSteadyOnApplication::m_cbrRate),
                          MakeDataRateChecker())
            .AddAttribute("PacketSize",
                          "The size of packets sent in on state",
                          UintegerValue(512),
                          MakeUintegerAccessor(&PrioSteadyOnApplication::m_pktSize),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("Remote",
                          "The address of the destination",
                          AddressValue(),
                          MakeAddressAccessor(&PrioSteadyOnApplication::m_peer),
                          MakeAddressChecker())
            .AddAttribute("Local",
                          "The Address on which to bind the socket. If not set, it is generated "
                          "automatically.",
                          AddressValue(),
                          MakeAddressAccessor(&PrioSteadyOnApplication::m_local),
                          MakeAddressChecker())
            .AddAttribute("ApplicationToS",
                          "The Type of Service used to send IPv4 packets. "
                          "All 8 bits of the TOS byte are set (including ECN bits).",
                          UintegerValue(0),
                          MakeUintegerAccessor(&PrioSteadyOnApplication::m_appTos),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("MaxBytes",
                          "The total number of bytes to send. Once these bytes are sent, "
                          "no packet is sent again, even in on state. The value zero means "
                          "that there is no limit.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&PrioSteadyOnApplication::m_maxBytes),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("Protocol",
                          "The type of protocol to use. This should be "
                          "a subclass of ns3::SocketFactory",
                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                          MakeTypeIdAccessor(&PrioSteadyOnApplication::m_tid),
                          // This should check for SocketFactory as a parent
                          MakeTypeIdChecker())
            .AddAttribute("FlowPriority",
                          "priority assigned to each flow.",
                          UintegerValue(0x1),
                          MakeUintegerAccessor(&PrioSteadyOnApplication::m_userSetPriority),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("MiceElephantProbability",
                          "The D parameter assigned to the flow by the user",
                          StringValue(""),
                          MakeStringAccessor(&PrioSteadyOnApplication::m_miceElephantProb),
                          MakeStringChecker())
            .AddAttribute("EnableSeqTsSizeHeader",
                          "Enable use of SeqTsSizeHeader for sequence number and timestamp",
                          BooleanValue(false),
                          MakeBooleanAccessor(&PrioSteadyOnApplication::m_enableSeqTsSizeHeader),
                          MakeBooleanChecker())
            .AddAttribute("Threshold",
                          "[packets], max number of packets per flow to be considered mouse flow",
                          UintegerValue(1),
                          MakeUintegerAccessor(&PrioSteadyOnApplication::m_threshold),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("StreamIndex",
                          "Stream index for RNG",
                          UintegerValue(0),
                          MakeUintegerAccessor(&PrioSteadyOnApplication::m_streamIndex),
                          MakeUintegerChecker<uint32_t>())
            .AddTraceSource("Tx",
                            "A new packet is created and is sent",
                            MakeTraceSourceAccessor(&PrioSteadyOnApplication::m_txTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("TxWithAddresses",
                            "A new packet is created and is sent",
                            MakeTraceSourceAccessor(&PrioSteadyOnApplication::m_txTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback")
            .AddTraceSource("TxWithSeqTsSize",
                            "A new packet is created with SeqTsSizeHeader",
                            MakeTraceSourceAccessor(&PrioSteadyOnApplication::m_txTraceWithSeqTsSize),
                            "ns3::PacketSink::SeqTsSizeCallback");
    return tid;
}

PrioSteadyOnApplication::PrioSteadyOnApplication()
    : m_socket(nullptr),
      m_connected(false),
      m_residualBits(0),
      m_lastStartTime(Seconds(0)),
      m_totBytes(0),
      m_unsentPacket(nullptr),
      m_packetSeqCount(1)
{
    NS_LOG_FUNCTION(this);
}

PrioSteadyOnApplication::~PrioSteadyOnApplication()
{
    NS_LOG_FUNCTION(this);
}

void
PrioSteadyOnApplication::SetMaxBytes(uint64_t maxBytes)
{
    NS_LOG_FUNCTION(this << maxBytes);
    m_maxBytes = maxBytes;
}

Ptr<Socket>
PrioSteadyOnApplication::GetSocket() const
{
    NS_LOG_FUNCTION(this);
    return m_socket;
}

int64_t
PrioSteadyOnApplication::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    return 0;
}

void
PrioSteadyOnApplication::DoDispose()
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
PrioSteadyOnApplication::StartApplication() // Called at time specified by Start
{
    NS_LOG_FUNCTION(this);

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

        m_socket->SetConnectCallback(MakeCallback(&PrioSteadyOnApplication::ConnectionSucceeded, this),
                                     MakeCallback(&PrioSteadyOnApplication::ConnectionFailed, this));

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
        m_lastStartTime = Simulator::Now();
        ScheduleNextTx(); // Start sending immediately
    }
}

void
PrioSteadyOnApplication::StopApplication() // Called at time specified by Stop
{
    NS_LOG_FUNCTION(this);

    CancelEvents();
    if (m_socket)
    {
        m_socket->Close();
    }
    else
    {
        NS_LOG_WARN("PrioSteadyOnApplication found null socket to close in StopApplication");
    }
}

void
PrioSteadyOnApplication::CancelEvents()
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
    // Canceling events may cause discontinuity in sequence number if the
    // SeqTsSizeHeader is header, and m_unsentPacket is true
    if (m_unsentPacket)
    {
        NS_LOG_DEBUG("Discarding cached packet upon CancelEvents ()");
    }
    m_unsentPacket = nullptr;
}

// Private helpers
void
PrioSteadyOnApplication::ScheduleNextTx()
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
        m_sendEvent = Simulator::Schedule(nextTime, &PrioSteadyOnApplication::SendPacket, this);
    }
    else
    { // All done, cancel any pending events
        StopApplication();
    }
}

void
PrioSteadyOnApplication::SendPacket()
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
        int32_t miceElephantProbValueDecInt = StringToDecInt(m_miceElephantProb);
        
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
    }

    SocketIpTosTag ipTosTag;
    int setPriorityMode = 1;
    switch (setPriorityMode)
    {
        case 1:
        {
            m_priority = m_userSetPriority;
            break;
        }
        case 2:
        {
            if (m_packetSeqCount < m_threshold)
            {
                m_priority = 0x1;
            }
            else 
                m_priority = 0x2;
            break;
        }
        case 3:
        {
            if (packet->PeekPacketTag(ipTosTag))
            {
                if (ipTosTag.GetTos() == 0x10)
                {
                    m_priority = 0x1;
                }
                else if (ipTosTag.GetTos() == 0x00 || ipTosTag.GetTos() == 0x02 || 
                         ipTosTag.GetTos() == 0x04 || ipTosTag.GetTos() == 0x06 || 
                         ipTosTag.GetTos() == 0x08)
                {
                    m_priority = 0x2;
                }
            }
            break;
        }
        default:
            break;
    }

    flowPrioTag.SetSimpleValue(m_priority);
    // store the tag in a packet.
    if (packet->PeekPacketTag(flowPrioTag) == false)
    {
        packet->AddPacketTag(flowPrioTag);
    }
    else
    {
        std::cout << "retransmit" << std::endl;
    }

    if (m_tid == ns3::TcpSocketFactory::GetTypeId())
    {
        ipTosTag.SetTos(m_appTos);
        appTosTag.SetTosValue(m_appTos);
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
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " steady-on application sent "
                                   << packet->GetSize() << " bytes to "
                                   << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() << " port "
                                   << InetSocketAddress::ConvertFrom(m_peer).GetPort()
                                   << " total Tx " << m_totBytes << " bytes");
            m_txTraceWithAddresses(packet, localAddress, InetSocketAddress::ConvertFrom(m_peer));
        }
        else if (Inet6SocketAddress::IsMatchingType(m_peer))
        {
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " steady-on application sent "
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
    
    m_lastStartTime = Simulator::Now();
    ScheduleNextTx();
}

void
PrioSteadyOnApplication::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    m_connected = true;
    m_lastStartTime = Simulator::Now();
    ScheduleNextTx(); // Start sending immediately upon connection
}

void
PrioSteadyOnApplication::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_FATAL_ERROR("Can't connect");
}

} // Namespace ns3
