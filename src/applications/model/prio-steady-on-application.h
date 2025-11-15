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
// Modified to create PrioSteadyOn - continuous traffic without ON/OFF periods
//

#ifndef PRIO_STEADY_ON_APPLICATION_H
#define PRIO_STEADY_ON_APPLICATION_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/data-rate.h"
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
 * \defgroup priosteadyon PrioSteadyOnApplication
 *
 * This traffic generator generates continuous CBR traffic at a specified data rate.
 * Unlike PrioOnOff, it does not have ON/OFF periods - traffic generation starts
 * when the application starts and continues until the application stops or MaxBytes is reached.
 */
/**
 * \ingroup priosteadyon
 *
 * \brief Generate continuous CBR traffic to a single destination with priority support.
 *
 * This traffic generator creates continuous traffic at a specified data rate.
 * Traffic generation starts immediately when Application::StartApplication is called
 * and continues until Application::StopApplication is called or MaxBytes limit is reached.
 *
 * The application generates CBR (Constant Bit Rate) traffic characterized by the
 * specified "data rate" and "packet size", with support for priority tagging.
 *
 * If the underlying socket type supports broadcast, this application
 * will automatically enable the SetAllowBroadcast(true) socket option.
 *
 * If the attribute "EnableSeqTsSizeHeader" is enabled, the application will
 * use some bytes of the payload to store a header with a sequence number,
 * a timestamp, and the size of the packet sent.
 */
class PrioSteadyOnApplication : public Application
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    PrioSteadyOnApplication();

    ~PrioSteadyOnApplication() override;

    /**
     * \brief Set the total number of bytes to send.
     *
     * Once these bytes are sent, no packet is sent again.
     * The value zero means that there is no limit.
     *
     * \param maxBytes the total number of bytes to send
     */
    void SetMaxBytes(uint64_t maxBytes);

    /**
     * \brief Return a pointer to associated socket.
     * \return pointer to associated socket
     */
    Ptr<Socket> GetSocket() const;

    /**
     * \brief Assign a fixed random variable stream number to the random variables
     * used by this model.
     *
     * \param stream first stream index to use
     * \return the number of stream indices assigned by this model
     */
    int64_t AssignStreams(int64_t stream);

  protected:
    void DoDispose() override;

  private:
    // inherited from Application base class.
    void StartApplication() override; // Called at time specified by Start
    void StopApplication() override;  // Called at time specified by Stop

    // helpers
    /**
     * \brief Cancel all pending events.
     */
    void CancelEvents();

    /**
     * \brief Send a packet
     */
    void SendPacket();

    Ptr<Socket> m_socket;                //!< Associated socket
    Address m_peer;                      //!< Peer address
    Address m_local;                     //!< Local address to bind to
    uint32_t m_appTos;                   //!< Type of Service. determined by user for the Application
    bool m_connected;                    //!< True if connected
    DataRate m_cbrRate;                  //!< Rate that data is generated
    DataRate m_cbrRateFailSafe;          //!< Rate that data is generated (check copy)
    uint32_t m_pktSize;                  //!< Size of packets
    uint32_t m_residualBits;             //!< Number of generated, but not sent, bits
    Time m_lastStartTime;                //!< Time last packet sent
    uint64_t m_maxBytes;                 //!< Limit total number of bytes sent
    uint64_t m_totBytes;                 //!< Total bytes sent so far
    EventId m_sendEvent;                 //!< Event id of pending "send packet" event
    TypeId m_tid;                        //!< Type of the socket used
    uint32_t m_seq{0};                   //!< Sequence
    Ptr<Packet> m_unsentPacket;          //!< Unsent packet cached for future attempt
    bool m_enableSeqTsSizeHeader{false}; //!< Enable or disable the use of SeqTsSizeHeader
    
    // flow prioritization Parameters
    uint8_t m_threshold;                 //!< [packets], max number of packets per flow to be considered mouse flow
    uint32_t m_userSetPriority;          //!< the priority assigned to each flow. defined externally by the user
    uint32_t m_priority;                 //!< the priority assigned to each packet/flow
    std::string m_miceElephantProb;      //!< the d parameter assigned to the flow by the user
    MiceElephantProbabilityTag miceElephantProbTag;  //!< Tag for mice/elephant probability
    SharedPriorityTag flowPrioTag;       //!< Tag for packet priority
    ApplicationTosTag appTosTag;         //!< Tag for application ToS
    uint64_t m_packetSeqCount;           //!< Number of packets sent in sequence
    uint32_t m_streamIndex;              //!< Stream index for RNG
    
    /// Traced Callback: transmitted packets.
    TracedCallback<Ptr<const Packet>> m_txTrace;

    /// Callbacks for tracing the packet Tx events, includes source and destination addresses
    TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_txTraceWithAddresses;

    /// Callback for tracing the packet Tx events, includes source, destination, the packet sent,
    /// and header
    TracedCallback<Ptr<const Packet>, const Address&, const Address&, const SeqTsSizeHeader&>
        m_txTraceWithSeqTsSize;

  private:
    /**
     * \brief Schedule the next packet transmission
     */
    void ScheduleNextTx();
    /**
     * \brief Handle a Connection Succeed event
     * \param socket the connected socket
     */
    void ConnectionSucceeded(Ptr<Socket> socket);
    /**
     * \brief Handle a Connection Failed event
     * \param socket the not connected socket
     */
    void ConnectionFailed(Ptr<Socket> socket);
};

} // namespace ns3

#endif /* PRIO_STEADY_ON_APPLICATION_H */
