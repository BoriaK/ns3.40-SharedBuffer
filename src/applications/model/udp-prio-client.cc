/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
 *
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
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 */
#include "udp-prio-client.h"

#include "seq-ts-header.h"

#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"

#include <cstdio>
#include <cstdlib>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UdpPrioClient");

NS_OBJECT_ENSURE_REGISTERED(UdpPrioClient);

TypeId
UdpPrioClient::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::UdpPrioClient")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<UdpPrioClient>()
            .AddAttribute(
                "MaxPackets",
                "The maximum number of packets the application will send (zero means infinite)",
                UintegerValue(100),
                MakeUintegerAccessor(&UdpPrioClient::m_count),
                MakeUintegerChecker<uint32_t>())
            .AddAttribute("Interval",
                          "The time to wait between packets",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&UdpPrioClient::m_interval),
                          MakeTimeChecker())
            .AddAttribute("RemoteAddress",
                          "The destination Address of the outbound packets",
                          AddressValue(),
                          MakeAddressAccessor(&UdpPrioClient::m_peerAddress),
                          MakeAddressChecker())
            .AddAttribute("RemotePort",
                          "The destination port of the outbound packets",
                          UintegerValue(100),
                          MakeUintegerAccessor(&UdpPrioClient::m_peerPort),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("PacketSize",
                          "Size of packets generated. The minimum packet size is 12 bytes which is "
                          "the size of the header carrying the sequence number and the time stamp.",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&UdpPrioClient::m_size),
                          MakeUintegerChecker<uint32_t>(12, 65507))
            .AddAttribute ("NumOfPacketsHighPrioThreshold", 
                        "The number of packets in a single sequence, up to which, "
                        "the flow will be tagged as High Priority. "
                        "If a generated flow is longer than this threshold, packets from it will be labaled as low priority",
                        UintegerValue (10),
                        MakeUintegerAccessor (&UdpPrioClient::m_threshold),
                        MakeUintegerChecker<uint8_t> ())
            .AddAttribute ("FlowPriority", 
                            "The priority assigned to all the packets created by this Application, "
                            "1 = high priority, 2 = low priority"
                            "if this attribute is added, it overrides other methods to define priority",
                            UintegerValue (0),
                            MakeUintegerAccessor (&UdpPrioClient::m_userSetPriority),
                            MakeUintegerChecker<uint32_t> ());
    return tid;
}

UdpPrioClient::UdpPrioClient()
{
    NS_LOG_FUNCTION(this);
    m_sent = 0;
    m_totalTx = 0;
    m_socket = nullptr;
    m_sendEvent = EventId();
    // m_threshold = 10;  // Flow classfication Threshold (length), Added by me!
    m_packetsSent = 0;
}

UdpPrioClient::~UdpPrioClient()
{
    NS_LOG_FUNCTION(this);
}

void
UdpPrioClient::SetRemote(Address ip, uint16_t port)
{
    NS_LOG_FUNCTION(this << ip << port);
    m_peerAddress = ip;
    m_peerPort = port;
}

void
UdpPrioClient::SetRemote(Address addr)
{
    NS_LOG_FUNCTION(this << addr);
    m_peerAddress = addr;
}

void
UdpPrioClient::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
}

void
UdpPrioClient::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (!m_socket)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
        {
            if (m_socket->Bind() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(
                InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
        else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
        {
            if (m_socket->Bind6() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(
                Inet6SocketAddress(Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
        else if (InetSocketAddress::IsMatchingType(m_peerAddress) == true)
        {
            if (m_socket->Bind() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(m_peerAddress);
        }
        else if (Inet6SocketAddress::IsMatchingType(m_peerAddress) == true)
        {
            if (m_socket->Bind6() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(m_peerAddress);
        }
        else
        {
            NS_ASSERT_MSG(false, "Incompatible address type: " << m_peerAddress);
        }
    }

#ifdef NS3_LOG_ENABLE
    std::stringstream peerAddressStringStream;
    if (Ipv4Address::IsMatchingType(m_peerAddress))
    {
        peerAddressStringStream << Ipv4Address::ConvertFrom(m_peerAddress);
    }
    else if (Ipv6Address::IsMatchingType(m_peerAddress))
    {
        peerAddressStringStream << Ipv6Address::ConvertFrom(m_peerAddress);
    }
    else if (InetSocketAddress::IsMatchingType(m_peerAddress))
    {
        peerAddressStringStream << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4();
    }
    else if (Inet6SocketAddress::IsMatchingType(m_peerAddress))
    {
        peerAddressStringStream << Inet6SocketAddress::ConvertFrom(m_peerAddress).GetIpv6();
    }
    m_peerAddressString = peerAddressStringStream.str();
#endif // NS3_LOG_ENABLE

    m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    m_socket->SetAllowBroadcast(true);
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &UdpPrioClient::Send, this);
}

void
UdpPrioClient::StopApplication()
{
    NS_LOG_FUNCTION(this);
    Simulator::Cancel(m_sendEvent);
}

void
UdpPrioClient::Send()
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT(m_sendEvent.IsExpired());
    SeqTsHeader seqTs;
    seqTs.SetSeq(m_sent);
    Ptr<Packet> p = Create<Packet>(m_size - (8 + 4)); // 8+4 : the size of the seqTs header
    p->AddHeader(seqTs);
      // create a tag.

    // option 1: set the priority tag arbitrarly from a user requested param:

    // option 2: set Tag value to depend on the number of previously sent packets:
    // if m_packetsSent < m_threshold: TagValue->0x0 (High Priority)
    // if m_packetsSent >= m_threshold: TagValue->0x1 (Low Priority)

    if (m_userSetPriority)
    {
        m_priority = m_userSetPriority;
    }
    else
    {
        m_packetsSent = m_sent;
        if (m_packetsSent < m_threshold)
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
    p->AddPacketTag (flowPrioTag);
    /////////////////////////

    if ((m_socket->Send(p)) >= 0)
    {
        ++m_sent;
        m_totalTx += p->GetSize();
#ifdef NS3_LOG_ENABLE
        NS_LOG_INFO("TraceDelay TX " << m_size << " bytes to " << m_peerAddressString << " Uid: "
                                     << p->GetUid() << " Time: " << (Simulator::Now()).As(Time::S));
#endif // NS3_LOG_ENABLE
    }
#ifdef NS3_LOG_ENABLE
    else
    {
        NS_LOG_INFO("Error while sending " << m_size << " bytes to " << m_peerAddressString);
    }
#endif // NS3_LOG_ENABLE

    if (m_sent < m_count || m_count == 0)
    {
        m_sendEvent = Simulator::Schedule(m_interval, &UdpPrioClient::Send, this);
    }
}

uint64_t
UdpPrioClient::GetTotalTx() const
{
    return m_totalTx;
}

} // Namespace ns3
