/*
 * Copyright (c) 2015 Natale Patriciello <natale.patriciello@gmail.com>
 *               2016 Stefano Avallone <stavallo@unina.it>
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
 */
#ifndef TRAFFICCONTROLLAYER_H
#define TRAFFICCONTROLLAYER_H

#include "ns3/address.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/queue-item.h"
#include "ns3/traced-callback.h"
////////added by me//////////////
#include "ns3/queue-size.h"
#include "ns3/traced-value.h"
#include "ns3/customTag.h"
////////////////////////////////

#include <map>
#include <vector>
#include <string>
#include <array>
#include <list>

namespace ns3
{

class Packet;
class QueueDisc;
class NetDeviceQueueInterface;

/// Priority map
typedef std::array<uint16_t, 16> TcPriomap;

/**
 * \defgroup traffic-control Traffic Control model
 */

/**
 * The Traffic Control layer aims at introducing an equivalent of the Linux Traffic
 * Control infrastructure into ns-3. The Traffic Control layer sits in between
 * the NetDevices (L2) and any network protocol (e.g., IP). It is in charge of
 * processing packets and performing actions on them: scheduling, dropping,
 * marking, policing, etc.
 *
 * \ingroup traffic-control
 *
 * \brief Traffic control layer class
 *
 * This object represents the main interface of the Traffic Control Module.
 * Basically, we manage both IN and OUT directions (sometimes called RX and TX,
 * respectively). The OUT direction is easy to follow, since it involves
 * direct calls: upper layer (e.g. IP) calls the Send method on an instance of
 * this class, which then calls the Enqueue method of the QueueDisc associated
 * with the device. The Dequeue method of the QueueDisc finally calls the Send
 * method of the NetDevice.
 *
 * The IN direction uses a little trick to reduce dependencies between modules.
 * In simple words, we use Callbacks to connect upper layer (which should register
 * their Receive callback through RegisterProtocolHandler) and NetDevices.
 *
 * An example of the IN connection between this layer and IP layer is the following:
 *\verbatim
  Ptr<TrafficControlLayer> tc = m_node->GetObject<TrafficControlLayer> ();

  NS_ASSERT (tc != 0);

  m_node->RegisterProtocolHandler (MakeCallback (&TrafficControlLayer::Receive, tc),
                                   Ipv4L3Protocol::PROT_NUMBER, device);
  m_node->RegisterProtocolHandler (MakeCallback (&TrafficControlLayer::Receive, tc),
                                   ArpL3Protocol::PROT_NUMBER, device);

  tc->RegisterProtocolHandler (MakeCallback (&Ipv4L3Protocol::Receive, this),
                               Ipv4L3Protocol::PROT_NUMBER, device);
  tc->RegisterProtocolHandler (MakeCallback (&ArpL3Protocol::Receive, PeekPointer
 (GetObject<ArpL3Protocol> ())), ArpL3Protocol::PROT_NUMBER, device); \endverbatim
 * On the node, for IPv4 and ARP packet, is registered the
 * TrafficControlLayer::Receive callback. At the same time, on the TrafficControlLayer
 * object, is registered the callbacks associated to the upper layers (IPv4 or ARP).
 *
 * When the node receives an IPv4 or ARP packet, it calls the Receive method
 * on TrafficControlLayer, that calls the right upper-layer callback once it
 * finishes the operations on the packet received.
 *
 * Discrimination through callbacks (in other words: what is the right upper-layer
 * callback for this packet?) is done through checks over the device and the
 * protocol number.
 */
class TrafficControlLayer : public Object
{
  public:
    struct TCStats
    {
      /// Total packets dropped before enqueue
      uint32_t nTotalDroppedPackets;
      /// Total High Pririty packets dropped before enqueue
      uint32_t nTotalDroppedPacketsHighPriority;  // added by me
      /// Total Low Pririty packets dropped before enqueue
      uint32_t nTotalDroppedPacketsLowPriority;  // added by me        
      /// Total bytes dropped before enqueue
      uint64_t nTotalDroppedBytes;
      /// Total High Pririty bytes dropped before enqueue
      uint64_t nTotalDroppedBytesHighPriority;  // added by me;
      /// Total Low Pririty bytes dropped before enqueue
      uint64_t nTotalDroppedBytesLowPriority;  // added by me;   

      /// constructor
      TCStats();

      // // destructor
      // ~TCStats();

      
      /**
       * \brief Print the statistics.
       * \param os output stream in which the data should be printed.
       */
      void Print(std::ostream& os) const;
    };

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * \brief Get the type ID for the instance
     * \return the instance TypeId
     */
    TypeId GetInstanceTypeId() const override;

    /**
     * \brief Constructor
     */
    TrafficControlLayer();

    ~TrafficControlLayer() override;

    // Delete copy constructor and assignment operator to avoid misuse
    TrafficControlLayer(const TrafficControlLayer&) = delete;
    TrafficControlLayer& operator=(const TrafficControlLayer&) = delete;

    /**
     * \brief Register an IN handler
     *
     * The handler will be invoked when a packet is received to pass it to
     * upper  layers.
     *
     * \param handler the handler to register
     * \param protocolType the type of protocol this handler is
     *        interested in. This protocol type is a so-called
     *        EtherType, as registered here:
     *        http://standards.ieee.org/regauth/ethertype/eth.txt
     *        the value zero is interpreted as matching all
     *        protocols.
     * \param device the device attached to this handler. If the
     *        value is zero, the handler is attached to all
     *        devices.
     */
    void RegisterProtocolHandler(Node::ProtocolHandler handler,
                                 uint16_t protocolType,
                                 Ptr<NetDevice> device);

    /// Typedef for queue disc vector
    typedef std::vector<Ptr<QueueDisc>> QueueDiscVector;

    /**
     * \brief Collect information needed to determine how to handle packets
     *        destined to each of the NetDevices of this node
     *
     * Checks whether a NetDeviceQueueInterface objects is aggregated to each of
     * the NetDevices of this node and sets the required callbacks properly.
     */
    virtual void ScanDevices();

    /**
     * \brief This method can be used to set the root queue disc installed on a device
     *
     * \param device the device on which the provided queue disc will be installed
     * \param qDisc the queue disc to be installed as root queue disc on device
     */
    virtual void SetRootQueueDiscOnDevice(Ptr<NetDevice> device, Ptr<QueueDisc> qDisc);

    /**
     * \brief This method can be used to get the root queue disc installed on a device
     *
     * \param device the device on which the requested root queue disc is installed
     * \return the root queue disc installed on the given device
     */
    virtual Ptr<QueueDisc> GetRootQueueDiscOnDevice(Ptr<NetDevice> device) const;

    /**
     * \brief This method can be used to remove the root queue disc (and associated
     *        filters, classes and queues) installed on a device
     *
     * \param device the device on which the installed queue disc will be deleted
     */
    virtual void DeleteRootQueueDiscOnDevice(Ptr<NetDevice> device);

    /**
     * \brief Set node associated with this stack.
     * \param node node to set
     */
    void SetNode(Ptr<Node> node);

    /**
     * \brief Called by NetDevices, incoming packet
     *
     * After analyses and scheduling, this method will call the right handler
     * to pass the packet up in the stack.
     *
     * \param device network device
     * \param p the packet
     * \param protocol next header value
     * \param from address of the correspondent
     * \param to address of the destination
     * \param packetType type of the packet
     */
    virtual void Receive(Ptr<NetDevice> device,
                         Ptr<const Packet> p,
                         uint16_t protocol,
                         const Address& from,
                         const Address& to,
                         NetDevice::PacketType packetType);
    /**
     * \brief Called from upper layer to queue a packet for the transmission.
     *
     * \param device the device the packet must be sent to
     * \param item a queue item including a packet and additional information
     */
    virtual void Send(Ptr<NetDevice> device, Ptr<QueueDiscItem> item);

  ///////Added by me to monitor dropped packets from TrafficControl Layer
    /**
     * \brief Retrieve all the collected statistics.
     * \return the collected statistics.
     */
    const TCStats& GetStats();
  /////////////////////////////////////////////////////////////////////  

  protected:
    void DoDispose() override;
    void DoInitialize() override;
    void NotifyNewAggregate() override;

  private:
    /**
     * \brief Protocol handler entry.
     * This structure is used to demultiplex all the protocols.
     */
    struct ProtocolHandlerEntry
    {
        Node::ProtocolHandler handler; //!< the protocol handler
        Ptr<NetDevice> device;         //!< the NetDevice
        uint16_t protocol;             //!< the protocol number
        bool promiscuous;              //!< true if it is a promiscuous handler
    };

    /**
     * \brief Information to store for each device
     */
    struct NetDeviceInfo
    {
        Ptr<QueueDisc> m_rootQueueDisc;      //!< the root queue disc on the device
        Ptr<NetDeviceQueueInterface> m_ndqi; //!< the netdevice queue interface
        QueueDiscVector m_queueDiscsToWake;  //!< the vector of queue discs to wake
    };

    /// Typedef for protocol handlers container
    typedef std::vector<ProtocolHandlerEntry> ProtocolHandlerList;

    /**
     * \brief Required by the object map accessor
     * \return the number of devices in the m_netDevices map
     */
    uint32_t GetNDevices() const;
    /**
     * \brief Required by the object map accessor
     * \param index the index of the device in the node's device list
     * \return the root queue disc installed on the specified device
     */
    Ptr<QueueDisc> GetRootQueueDiscOnDeviceByIndex(uint32_t index) const;

  //////////////Added by me//////////////////////////////////////////////////////
    /**
     * \brief Get the index of the current port (net-device) from the pointer
     * \returns index of the current port (net-device).
     */
    size_t GetNetDeviceIndex(Ptr<NetDevice> device);

    /**
     * \brief Get the current size of the shared queue disc in bytes, if
     *        operating in bytes mode, or packets, otherwise.
     * it iterates over all the NetDevices that are aggregated to the Node
     * and sums all the packets on all of them
     *
     * \returns The Shared Buffer size in bytes or packets.
     */
    QueueSize GetCurrentSharedBufferSize();

    /**
     * \brief Get the maximum size of the Shared Buffer.
     *
     * \returns the maximum size of the queue disc.
     */
    QueueSize GetMaxSharedBufferSize() const;

    /**
     * \brief Get the queueing limit of the current queue for each priority alpha, for DT Algorithm.
     * \param alpha the alpha parameter relevant to the current arriving packet
     * \param alpha_h the pre-determined hyperparameter alpha High
     * \param alpha_l the pre-determined hyperparameter alpha Low
     * \returns the maximum number of packets allowed in the queue.
     */
    QueueSize GetQueueThreshold_DT (double_t alpha, double_t alpha_h, double_t alpha_l);

    /**
     * \brief Get the queueing limit of the current queue for each priority alpha, for FB Algorithm. v2 excepts Np(t) as an external variable
     * \param alpha the alpha parameter relevant to the current arriving packet
     * \param alpha_h the pre-determined hyperparameter alpha High
     * \param alpha_l the pre-determined hyperparameter alpha Low
     * \param conjestedQueues
     * \param gamma_i the normalized dequeue rate on port i
     * \returns the maximum number of packets allowed in the queue.
     */
    QueueSize GetQueueThreshold_FB_v2 (double_t alpha, double_t alpha_h, double_t alpha_l, int conjestedQueues, float gamma_i);

    /**
     * \brief Get the queueing limit of the current queue for each priority alpha, for FB Algorithm. v2 excepts Np(t) as an external variable
     * \param queue_priority the priority of the queue that's currently being checked
     * \param alpha_h the pre-determined hyperparameter alpha High
     * \param alpha_l the pre-determined hyperparameter alpha Low
     * \param conjestedQueues
     * \param gamma_i the normalized dequeue rate on port i
     * \returns the maximum number of packets allowed in the queue.
     */
    QueueSize GetQueueThreshold_Predictive_FB_v1 (uint32_t queue_priority, double_t alpha_h, double_t alpha_l, int conjestedQueues, float gamma_i);

    /**
     * \brief Get the current number of High Priority Packets in the net-device (port) queue that the current packet is sent to, 
     *        in bytes, ifoperating in bytes mode, or packets, otherwise.
     *
     * \returns The number of high priority packets in queue disc in bytes or packets.
     */
    QueueSize GetNumOfHighPriorityPacketsInSharedQueue();

    /**
     * \brief Get the current number of Low Priority Packets in the net-device (port) queue that the current packet is sent to, 
     *        in bytes, ifoperating in bytes mode, or packets, otherwise..
     *
     *
     * \returns The number of high priority packets in queue disc in bytes or packets.
     */
    QueueSize GetNumOfLowPriorityPacketsInSharedQueue();

    /**
     * \brief Get the current number of conjested (non empty) queues on the shared buffer, 
     *        in bytes, ifoperating in bytes mode, or packets, otherwise. for a SINGLE QUEUE PER PORT architecture
     * \returns The number of high priority packets in queue disc in bytes or packets.
     */
    uint32_t GetNumOfConjestedQueuesInSharedQueue_v1();

    /**
     * \brief Get the current number of conjested (non empty) queues of priority p on the shared buffer, 
     *        in bytes, ifoperating in bytes mode, or packets, otherwise. for a MULTIQUEUE PER PORT architecture.
     * \param queue_priority the priority of the queue that's currently being checked
     * \returns The number of high priority packets in queue disc in bytes or packets.
     */
    uint32_t GetNumOfPriorityConjestedQueuesInSharedQueue_v1(uint32_t queue_priority);

    /**
     * \brief calculate the normalized dequeue rate of queue_i(t). gamma_i(t) 
     *        at the current model, each queue on the port has an equal share of the dequeue BW.
     *        so the actual gamma_i(t) is calculated as: 1/current number of non empty queues on the port.
     * \returns the normalized dequeue rate of queue_i(t).
     */
    float_t GetNormalizedDequeueBandWidth(Ptr<NetDevice> device, uint8_t queueIndex);

    /**
     * \brief claculate the nesesarry learning rate as the normalized difference between the incoming traffic
     *        in time t+Tau and t
     * \returns mue(t, t+Tau) the learniing rate for the predictive model.
     */
    float_t GetLearningRate();

    /**
     * \brief calculate the number of lost packets of priority P, during the time interval t: t+Tau if
     *        FB with current alpha High/Low values is being used.
     * \param queue_priority the priority of the queue that's currently being checked
     * \returns the number of lost packets of priority P.
     */
    float_t GetNumOfLostPackets(uint32_t queue_priority);

    /**
     * \brief delta Aplha is the optimal difference between alpha high and alpha low based on the predictive model.
     * \param alpha_h the pre-determined hyperparameter alpha High
     * \param alpha_l the pre-determined hyperparameter alpha Low
     * \param queue_priority the priority of the queue that's currently being checked
     * \returns return the new alpha_high - alpha_low value based on the predictive model.
     */
    double_t GetNewDeltaAlpha(double_t alpha_h, double_t alpha_l, uint32_t queue_priority);

        /**
     * \brief returns the optimal Alpha High and Low values based on the D of the traffic.
     * \param miceElephantProbVal the mice/elephant probability (D) assigned at the OnOff Application
    //  * \param queue_priority the priority of the queue that's currently being checked
     * \returns return the new alpha_high and alpha_low value based on the optimization done prior.
     */
    std::pair<double_t, double_t> GetNewAlphaHighAndLow(uint32_t miceElephantProbVal);
    // double_t GetNewAlphaHighAndLow(uint32_t miceElephantProbVal, uint32_t queue_priority);
  //////////////////////////////////////////////////////////////////////////////
    /**
     * \brief Perform the actions required when the queue disc is notified of
     *        a packet dropped before enqueue
     * \param item item that was dropped
     * \param reason the reason why the item was dropped
     * This method must be called by subclasses to record that a packet was
     * dropped before enqueue for the specified reason
     */
    void DropBeforeEnqueue(Ptr<const QueueDiscItem> item);
    
    //// The node this TrafficControlLayer object is aggregated to
    Ptr<Node> m_node;
    /// Map storing the required information for each device with a queue disc installed
    std::map<Ptr<NetDevice>, NetDeviceInfo> m_netDevices;
    ProtocolHandlerList m_handlers; //!< List of upper-layer handlers
  ///// for Shared Buffer Queue. trace the packets in queue ////////////////////////////
    QueueSize m_maxSharedBufferSize; // !< only in case of Shared-Buffer, the max size of shared buffer queue
    uint32_t trafficControllPacketCounter = 0;
    uint32_t trafficControllBytesCounter = 0;
    uint32_t m_sharedBufferPackets = 0;
    uint32_t m_sharedBufferBytes = 0;
    TracedValue<uint32_t> m_traceSharedBufferPackets;
    TracedValue<uint32_t> m_traceSharedBufferBytes;
    uint32_t m_nPackets_High_InSharedQueue; //!< Number of High Priority packets in the shared-queue ######## Added by me!######
    uint32_t m_nPackets_Low_InSharedQueue; //!< Number of Low Priority packets in the queue ######## Added by me!######
    uint32_t m_nBytes_High_InSharedQueue; //!< Number of High Priority packets in the shared-queue ######## Added by me!######
    uint32_t m_nBytes_Low_InSharedQueue; //!< Number of Low Priority packets in the queue ######## Added by me!######  
    TracedValue<uint32_t> m_nPackets_trace_High_InSharedQueue;
    TracedValue<uint32_t> m_nPackets_trace_Low_InSharedQueue;
    // general parameters:
    TcPriomap m_tosPrioMap; // the priority map used by the Round Robin Prio-QueueDisc
    std::string m_usedAlgorythm; // the Traffic Controll algorythm to be used to manage traffic in shared buffer
    bool m_useSharedBuffer; // !< True if Shared-Buffer is used
    bool m_multiQueuePerPort; // !< True if multi-queue/port is used
    float_t m_p_threshold_h; //!< Maximum number of packets enqueued for high priority stream ### Added BY ME ####
    float_t m_p_threshold_l; //!< Maximum number of packets enqueued for low priority stream ### Added BY ME ####
    TracedValue<float_t> m_p_trace_threshold_h_0; //!< Maximum number of packets enqueued for high priority stream ### Added BY ME ####
    TracedValue<float_t> m_p_trace_threshold_h_1; //!< Maximum number of packets enqueued for high priority stream ### Added BY ME ####
    TracedValue<float_t> m_p_trace_threshold_l_0; //!< Maximum number of packets enqueued for low priority stream ### Added BY ME ####
    TracedValue<float_t> m_p_trace_threshold_l_1; //!< Maximum number of packets enqueued for low priority stream ### Added BY ME ####
    TracedValue<float_t> m_b_threshold_h; //!< Maximum number of bytes enqueued for high priority stream ### Added BY ME ####
    TracedValue<float_t> m_b_threshold_l; //!< Maximum number of bytes enqueued for low priority stream ### Added BY ME ####
    double_t m_alpha;  //!< The selected alpha parameter for the arriving packet
    double_t m_alpha_h; //!< The pre-determined alpha parameter for high priority packets
    double_t m_alpha_l;  //!< The pre-determined alpha parameter for low priority packets
    bool m_adjustableAlphas; // !< True if apply optimal Alpha High/Low values based on current "D"
    // FB queue disc parameters:
    float_t m_gamma;  //!< Normalized de-queue rate per port/queue in a single FIFO per port scenario.
    uint8_t m_nonEmpty; //!< number of non empty queues on each port.
    // Predictive queue disc parameters:
    float_t m_mue;  //!< Normalized learning rate of the predictive queue managment algorythm.
    float_t m_lostPackets;  //!< the number lost packets in SharedBuffer of priority P in the time inerval t:t+Tau.
    double_t predictedPacketsInSharedBuffer; //!< the predicted number of packets in shared buffer in time t+Tau
    double_t predictedHighPriorityPacketsInSharedBuffer; //!< the predicted number of High Priority packets in shared buffer in time t+Tau
    double_t predictedLowPriorityPacketsInSharedBuffer; //!< the predicted number of Low Priority packets in shared buffer in time t+Tau
    double_t predictedPacketsLostInSharedBuffer; //!< the predicted number of packets Lost in shared buffer in time t+Tau
    double_t predictedHighPriorityPacketsLostInSharedBuffer; //!< the predicted number of High Priority packets Lost in shared buffer in time t+Tau
    double_t predictedLowPriorityPacketsLostInSharedBuffer; //!< the predicted number of Low Priority packets Lost in shared buffer in time t+Tau
    double_t m_deltaAlpha; //!< the delta between the alpha parameters
    double_t m_deltaAlphaUpdate; //!< the updated delta between the alpha parameters
    // uint8_t queue_priority;   //< the priority of the queue that's currently being checked
    uint32_t m_nConjestedQueues;   //!< number of queues that are conjested at current time instance
    uint32_t m_nConjestedQueues_p;   //!< number of queues of priority p that are conjested at current time instance
    float_t m_numConjestedQueues;  //!< total number of queues that are conjested at current time instance
    float_t m_numConjestedQueuesHigh;  //!< number of queues that are conjested at current time instance
    float_t m_numConjestedQueuesLow;  //!< number of queues that are conjested at current time instance
    float_t numOfClasses;  //!< total number of classes. for now it's only High Priority/ Low Priority
    // Flow Classification Parameters
    MiceElephantProbabilityTag miceElephantProbTag;  //!< a Tag that represents the mice/elephant probability assigned to the flow by the user
    double_t m_miceElephantProbVal;  //!< the actual d value of the flow that was assigned at the OnOff application
    SharedPriorityTag flowPrioTag;    //< a tag that's added to each sent packet based on the priority assigned by the Sender application
    uint8_t m_flow_priority;   //< Flow priority assigned to each recieved packet, based on the flow priority assigned by sender
    // to collect statistics at the end of the flow
    TCStats m_stats;    //!< The collected statistics
////////////////////////////////////////////////////////////////////////////////////

    /**
     * The trace source fired when the Traffic Control layer drops a packet because
     * no queue disc is installed on the device, the device supports flow control and
     * the device queue is stopped
     */
    TracedCallback<Ptr<const Packet>> m_dropped;
};

/**
 * \brief Stream insertion operator.
 *
 * \param os the stream
 * \param stats the queue disc statistics
 * \returns a reference to the stream
 */
std::ostream& operator<<(std::ostream& os, const TrafficControlLayer::TCStats& stats);

/**
 * Serialize the priomap to the given ostream
 *
 * \param os
 * \param priomap
 *
 * \return std::ostream
 */
std::ostream& operator<<(std::ostream& os, const TcPriomap& priomap);

/**
 * Serialize from the given istream to this priomap.
 *
 * \param is
 * \param priomap
 *
 * \return std::istream
 */
std::istream& operator>>(std::istream& is, TcPriomap& priomap);

ATTRIBUTE_HELPER_HEADER(TcPriomap);

} // namespace ns3

#endif // TRAFFICCONTROLLAYER_H
