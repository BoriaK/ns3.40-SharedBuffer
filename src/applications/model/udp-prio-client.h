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
 *
 */

#ifndef UDP_PRIO_CLIENT_H
#define UDP_PRIO_CLIENT_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-address.h"
#include "ns3/ptr.h"
#include "ns3/customTag.h"

namespace ns3
{

class Socket;
class Packet;

/**
 * \ingroup udpclientserver
 *
 * \brief A Udp client. Sends UDP packet carrying sequence number and time stamp
 *  in their payloads
 *
 */
class UdpPrioClient : public Application
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    UdpPrioClient();

    ~UdpPrioClient() override;

    /**
     * \brief set the remote address and port
     * \param ip remote IP address
     * \param port remote port
     */
    void SetRemote(Address ip, uint16_t port);
    /**
     * \brief set the remote address
     * \param addr remote address
     */
    void SetRemote(Address addr);

    /**
     * \return the total bytes sent by this app
     */
    uint64_t GetTotalTx() const;

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * \brief Send a packet
     */
    void Send();

    uint32_t m_count; //!< Maximum number of packets the application will send
    Time m_interval;  //!< Packet inter-send time
    uint32_t m_size;  //!< Size of the sent packet (including the SeqTsHeader)

    uint32_t m_sent;       //!< Counter for sent packets
    uint64_t m_totalTx;    //!< Total bytes sent
    Ptr<Socket> m_socket;  //!< Socket
    Address m_peerAddress; //!< Remote peer address
    uint16_t m_peerPort;   //!< Remote peer port
    EventId m_sendEvent;   //!< Event to send the next packet
#ifdef NS3_LOG_ENABLE
    std::string m_peerAddressString; //!< Remote peer address string
#endif                               // NS3_LOG_ENABLE
    uint32_t        m_packetsSent;  //!< The number of pacts sent.
    uint32_t        m_userSetPriority;     //!< the priority assigned to each flow. defined externaly by the user
    uint32_t        m_priority;     //!< the priority assigned to each packet/ flow. can be user defined or based on flow length.

    // flow prioritization Parameters
    uint8_t         m_threshold;     //< [packets], max number of packets per flow to be considered mouse flow
    SharedPriorityTag           flowPrioTag;   //< a tag that's added to each sent packet based on the priority assigned by the SendPacket () function
};

} // namespace ns3

#endif /* UDP_PRIO_CLIENT_H */
