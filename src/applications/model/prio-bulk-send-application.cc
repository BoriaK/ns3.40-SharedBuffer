/*
 * Copyright (c) 2010 Georgia Institute of Technology
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
 * Author: George F. Riley <riley@ece.gatech.edu>
 * Modified to add priority support
 */

#include "prio-bulk-send-application.h"

#include "ns3/address.h"
#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"

#include <iostream>
#include <string>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("PrioBulkSendApplication");

NS_OBJECT_ENSURE_REGISTERED(PrioBulkSendApplication);

/**
 * \brief Convert string to double
 * \param value the string value to convert
 * \return the double value
 */
// static double
// StringToDouble(std::string value)
// {
//     return std::stod(value);
// }

/**
 * \brief Convert string to decimal integer (multiplied by 10)
 * \param value the string value to convert
 * \return the integer value multiplied by 10
 */
static int32_t
StringToDecInt(std::string value)
{
    return static_cast<int32_t>(std::stod(value) * 10);
}

TypeId
PrioBulkSendApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::PrioBulkSendApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<PrioBulkSendApplication>()
            .AddAttribute("SendSize",
                          "The amount of data to send each time.",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&PrioBulkSendApplication::m_sendSize),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("Remote",
                          "The address of the destination",
                          AddressValue(),
                          MakeAddressAccessor(&PrioBulkSendApplication::m_peer),
                          MakeAddressChecker())
            .AddAttribute("Local",
                          "The Address on which to bind the socket. If not set, it is generated "
                          "automatically.",
                          AddressValue(),
                          MakeAddressAccessor(&PrioBulkSendApplication::m_local),
                          MakeAddressChecker())
            .AddAttribute("MaxBytes",
                          "The total number of bytes to send. "
                          "Once these bytes are sent, "
                          "no data is sent again. The value zero means "
                          "that there is no limit.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&PrioBulkSendApplication::m_maxBytes),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("Protocol",
                          "The type of protocol to use.",
                          TypeIdValue(TcpSocketFactory::GetTypeId()),
                          MakeTypeIdAccessor(&PrioBulkSendApplication::m_tid),
                          MakeTypeIdChecker())
            .AddAttribute("EnableSeqTsSizeHeader",
                          "Add SeqTsSizeHeader to each packet",
                          BooleanValue(false),
                          MakeBooleanAccessor(&PrioBulkSendApplication::m_enableSeqTsSizeHeader),
                          MakeBooleanChecker())
            .AddAttribute("ApplicationToS",
                          "The Type of Service for all packets sent by this application.",
                          UintegerValue(0x0),
                          MakeUintegerAccessor(&PrioBulkSendApplication::m_appTos),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("NumOfPacketsHighPrioThreshold",
                          "The number of packets in a single sequence, up to which "
                          "the flow will be tagged as High Priority. "
                          "If a generated flow is longer than this threshold, packets from it will "
                          "be labeled as low priority",
                          UintegerValue(10),
                          MakeUintegerAccessor(&PrioBulkSendApplication::m_threshold),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("FlowPriority",
                          "The priority assigned to all the packets created by this Application. "
                          "1 = high priority, 2 = low priority. "
                          "If this attribute is added, it overrides other methods to define priority",
                          UintegerValue(0),
                          MakeUintegerAccessor(&PrioBulkSendApplication::m_userSetPriority),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("MiceElephantProbability",
                          "The parameter D that's configured by the user. "
                          "High D -> high mice probability, Low D -> high elephant probability. "
                          "If this attribute is added, the application will add a MiceElephantProbTag "
                          "with the assigned D value",
                          StringValue(""),
                          MakeStringAccessor(&PrioBulkSendApplication::m_miceElephantProb),
                          MakeStringChecker())
            .AddTraceSource("Tx",
                            "A new packet is sent",
                            MakeTraceSourceAccessor(&PrioBulkSendApplication::m_txTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("TxWithSeqTsSize",
                            "A new packet is created with SeqTsSizeHeader",
                            MakeTraceSourceAccessor(&PrioBulkSendApplication::m_txTraceWithSeqTsSize),
                            "ns3::PacketSink::SeqTsSizeCallback");
    return tid;
}

PrioBulkSendApplication::PrioBulkSendApplication()
    : m_socket(nullptr),
      m_connected(false),
      m_totBytes(0),
      m_unsentPacket(nullptr),
      m_packetCount(0)
{
    NS_LOG_FUNCTION(this);
}

PrioBulkSendApplication::~PrioBulkSendApplication()
{
    NS_LOG_FUNCTION(this);
}

void
PrioBulkSendApplication::SetMaxBytes(uint64_t maxBytes)
{
    NS_LOG_FUNCTION(this << maxBytes);
    m_maxBytes = maxBytes;
}

Ptr<Socket>
PrioBulkSendApplication::GetSocket() const
{
    NS_LOG_FUNCTION(this);
    return m_socket;
}

void
PrioBulkSendApplication::DoDispose()
{
    NS_LOG_FUNCTION(this);

    m_socket = nullptr;
    m_unsentPacket = nullptr;
    // chain up
    Application::DoDispose();
}

// Application Methods
void
PrioBulkSendApplication::StartApplication() // Called at time specified by Start
{
    NS_LOG_FUNCTION(this);
    Address from;

    // Create the socket if not already
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), m_tid);
        int ret = -1;

        // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
        if (m_socket->GetSocketType() != Socket::NS3_SOCK_STREAM &&
            m_socket->GetSocketType() != Socket::NS3_SOCK_SEQPACKET)
        {
            NS_FATAL_ERROR("Using PrioBulkSend with an incompatible socket type. "
                           "PrioBulkSend requires SOCK_STREAM or SOCK_SEQPACKET. "
                           "In other words, use TCP instead of UDP.");
        }

        // Set the ToS on the socket
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
            else if (InetSocketAddress::IsMatchingType(m_peer))
            {
                ret = m_socket->Bind();
            }
        }

        if (ret == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }

        m_socket->Connect(m_peer);
        m_socket->ShutdownRecv();
        m_socket->SetConnectCallback(MakeCallback(&PrioBulkSendApplication::ConnectionSucceeded, this),
                                     MakeCallback(&PrioBulkSendApplication::ConnectionFailed, this));
        m_socket->SetSendCallback(MakeCallback(&PrioBulkSendApplication::DataSend, this));
    }
    if (m_connected)
    {
        m_socket->GetSockName(from);
        SendData(from, m_peer);
    }
}

void
PrioBulkSendApplication::StopApplication() // Called at time specified by Stop
{
    NS_LOG_FUNCTION(this);

    if (m_socket)
    {
        m_socket->Close();
        m_connected = false;
    }
    else
    {
        NS_LOG_WARN("PrioBulkSendApplication found null socket to close in StopApplication");
    }
}

// Private helpers

void
PrioBulkSendApplication::SendData(const Address& from, const Address& to)
{
    NS_LOG_FUNCTION(this);

    while (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    { // Time to send more

        // uint64_t to allow the comparison later.
        // the result is in a uint32_t range anyway, because
        // m_sendSize is uint32_t.
        uint64_t toSend = m_sendSize;
        // Make sure we don't send too many
        if (m_maxBytes > 0)
        {
            toSend = std::min(toSend, m_maxBytes - m_totBytes);
        }

        NS_LOG_LOGIC("sending packet at " << Simulator::Now());

        Ptr<Packet> packet;
        if (m_unsentPacket)
        {
            packet = m_unsentPacket;
            toSend = packet->GetSize();
        }
        else if (m_enableSeqTsSizeHeader)
        {
            SeqTsSizeHeader header;
            header.SetSeq(m_seq++);
            header.SetSize(toSend);
            NS_ABORT_IF(toSend < header.GetSerializedSize());
            packet = Create<Packet>(toSend - header.GetSerializedSize());
            // Trace before adding header, for consistency with PacketSink
            m_txTraceWithSeqTsSize(packet, from, to, header);
            packet->AddHeader(header);
        }
        else
        {
            packet = Create<Packet>(toSend);
        }

        // Add MiceElephantProbability tag if configured
        if (!m_miceElephantProb.empty())
        {
            int32_t miceElephantProbValueDecInt = StringToDecInt(m_miceElephantProb);
            miceElephantProbTag.SetSimpleValue(miceElephantProbValueDecInt);
            
            if (!packet->PeekPacketTag(miceElephantProbTag))
            {
                packet->AddPacketTag(miceElephantProbTag);
            }
        }

        // Determine priority
        int setPriorityMode = 1; // Default to user-set priority mode
        switch (setPriorityMode)
        {
            ////////////////// option 1: set the priority tag arbitrarily from a user requested param
            case 1:
            {
                m_priority = m_userSetPriority;
                break;
            }
            ////////////////// option 2: add priority tag to packet based on packet count
            // if m_packetCount < m_threshold: TagValue->0x1 (High Priority)
            // if m_packetCount >= m_threshold: TagValue->0x2 (Low Priority)
            case 2:
            {
                if (m_packetCount < m_threshold)
                {
                    m_priority = 0x1;
                }
                else
                {
                    m_priority = 0x2;
                }
                break;
            }
            /////////////////////// option 3: add a priority to a packet based on ToS value.
            // ToS 0x0 -> Low Priority -> TagValue->0x2 (Low Priority)
            // ToS 0x10 -> High Priority -> TagValue->0x1 (High Priority)
            case 3:
            {
                SocketIpTosTag ipTosTag;
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

        // Add priority tag to packet
        flowPrioTag.SetSimpleValue(m_priority);
        if (!packet->PeekPacketTag(flowPrioTag))
        {
            packet->AddPacketTag(flowPrioTag);
        }

        // Add Application ToS tag for TCP
        if (m_tid == TcpSocketFactory::GetTypeId())
        {
            appTosTag.SetTosValue(m_appTos);
            if (!packet->PeekPacketTag(appTosTag))
            {
                packet->AddPacketTag(appTosTag);
            }
        }

        int actual = m_socket->Send(packet);
        if ((unsigned)actual == toSend)
        {
            m_totBytes += actual;
            m_packetCount++;
            m_txTrace(packet);
            m_unsentPacket = nullptr;
        }
        else if (actual == -1)
        {
            // We exit this loop when actual < toSend as the send side
            // buffer is full. The "DataSent" callback will pop when
            // some buffer space has freed up.
            NS_LOG_DEBUG("Unable to send packet; caching for later attempt");
            m_unsentPacket = packet;
            break;
        }
        else if (actual > 0 && (unsigned)actual < toSend)
        {
            // A Linux socket (non-blocking, such as in DCE) may return
            // a quantity less than the packet size. Split the packet
            // into two, trace the sent packet, save the unsent packet
            NS_LOG_DEBUG("Packet size: " << packet->GetSize() << "; sent: " << actual
                                         << "; fragment saved: " << toSend - (unsigned)actual);
            Ptr<Packet> sent = packet->CreateFragment(0, actual);
            Ptr<Packet> unsent = packet->CreateFragment(actual, (toSend - (unsigned)actual));
            m_totBytes += actual;
            m_packetCount++;
            m_txTrace(sent);
            m_unsentPacket = unsent;
            break;
        }
        else
        {
            NS_FATAL_ERROR("Unexpected return value from m_socket->Send ()");
        }
    }
    // Check if time to close (all sent)
    if (m_totBytes == m_maxBytes && m_connected)
    {
        m_socket->Close();
        m_connected = false;
    }
}

void
PrioBulkSendApplication::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_LOGIC("PrioBulkSendApplication Connection succeeded");
    m_connected = true;
    Address from;
    Address to;
    socket->GetSockName(from);
    socket->GetPeerName(to);
    SendData(from, to);
}

void
PrioBulkSendApplication::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_LOGIC("PrioBulkSendApplication, Connection Failed");
}

void
PrioBulkSendApplication::DataSend(Ptr<Socket> socket, uint32_t)
{
    NS_LOG_FUNCTION(this);

    if (m_connected)
    { // Only send new data if the connection has completed
        Address from;
        Address to;
        socket->GetSockName(from);
        socket->GetPeerName(to);
        SendData(from, to);
    }
}

} // Namespace ns3
