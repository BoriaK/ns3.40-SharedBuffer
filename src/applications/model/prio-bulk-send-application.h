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

#ifndef PRIO_BULK_SEND_APPLICATION_H
#define PRIO_BULK_SEND_APPLICATION_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/seq-ts-size-header.h"
#include "ns3/traced-callback.h"
#include "ns3/customTag.h"

namespace ns3
{

class Address;
class Socket;

/**
 * \ingroup applications
 * \defgroup priobulksend PrioBulkSendApplication
 *
 * This traffic generator sends data to a TCP/UDP socket as fast as 
 * the network can consume it. This is useful for simulating bulk data
 * transfers with priority tagging support.
 */
/**
 * \ingroup priobulksend
 *
 * \brief Send as much traffic as possible to a single address with priority support.
 *
 * This traffic generator simply sends data to a TCP or UDP socket as fast as
 * the network can consume it. Once Application::StartApplication() is called,
 * it will send "m_sendSize" bytes at a time to the socket until either
 * "m_maxBytes" bytes are sent or the Application is stopped.
 *
 * For TCP sockets, the application will respect flow control and send data
 * as fast as TCP allows. For UDP sockets, data is sent continuously without
 * flow control.
 *
 * Similar to BulkSendApplication but with added priority tagging capabilities
 * for SharedBuffer queue management.
 */
class PrioBulkSendApplication : public Application
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    PrioBulkSendApplication();

    ~PrioBulkSendApplication() override;

    /**
     * \brief Set the upper bound for the total number of bytes to send.
     * \param maxBytes the upper bound of bytes to send
     */
    void SetMaxBytes(uint64_t maxBytes);

    /**
     * \brief Get the socket this application is attached to.
     * \return pointer to associated socket
     */
    Ptr<Socket> GetSocket() const;

  protected:
    void DoDispose() override;

  private:
    // inherited from Application base class.
    void StartApplication() override; // Called at time specified by Start
    void StopApplication() override;  // Called at time specified by Stop

    /**
     * \brief Send data until the L4 transmission buffer is full.
     * \param from the address sending from
     * \param to the address sending to
     */
    void SendData(const Address& from, const Address& to);

    Ptr<Socket> m_socket;       //!< Associated socket
    Address m_peer;             //!< Peer address
    Address m_local;            //!< Local address to bind to (optional)
    bool m_connected;           //!< True if connected
    uint32_t m_sendSize;        //!< Size of data to send each time
    uint64_t m_maxBytes;        //!< Limit total number of bytes sent
    uint64_t m_totBytes;        //!< Total bytes sent so far
    TypeId m_tid;               //!< The type of protocol to use
    uint32_t m_seq{0};          //!< Sequence number
    Ptr<Packet> m_unsentPacket; //!< Packet we attempted to send, but was buffered
    bool m_enableSeqTsSizeHeader{false}; //!< Enable or disable SeqTsSizeHeader
    uint32_t m_appTos;           //!< Type of Service, determined by user for the Application
    
    // Priority-related attributes
    uint8_t m_threshold;        //!< [packets], max number of packets per flow to be considered mouse flow
    uint32_t m_userSetPriority; //!< The priority assigned to each flow, defined externally by the user
    uint32_t m_priority;        //!< The priority assigned to each packet/flow
    std::string m_miceElephantProb; //!< The D parameter assigned to the flow by the user
    uint64_t m_packetCount;     //!< Number of packets sent
    
    // Tags for priority marking
    MiceElephantProbabilityTag miceElephantProbTag; //!< Tag that represents mice/elephant probability
    SharedPriorityTag flowPrioTag;                   //!< Tag added to each packet based on priority
    ApplicationTosTag appTosTag;                     //!< Tag added to each packet based on ToS

    /// Traced Callback: transmitted packets.
    TracedCallback<Ptr<const Packet>> m_txTrace;

    /// Callbacks for tracing the packet Tx events with SeqTsSizeHeader
    TracedCallback<Ptr<const Packet>, const Address&, const Address&, const SeqTsSizeHeader&>
        m_txTraceWithSeqTsSize;

  private:
    /**
     * \brief Connection Succeeded (called by Socket through a callback)
     * \param socket the connected socket
     */
    void ConnectionSucceeded(Ptr<Socket> socket);
    /**
     * \brief Connection Failed (called by Socket through a callback)
     * \param socket the connected socket
     */
    void ConnectionFailed(Ptr<Socket> socket);
    /**
     * \brief Send more data as soon as some has been transmitted.
     * \param socket the socket
     * \param availableBufferSize available buffer size (unused)
     */
    void DataSend(Ptr<Socket> socket, uint32_t availableBufferSize);
};

} // namespace ns3

#endif /* PRIO_BULK_SEND_APPLICATION_H */
