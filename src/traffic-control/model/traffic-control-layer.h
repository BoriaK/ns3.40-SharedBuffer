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
//////////////////////////////////////////////////////////////////////////////////
// Custom data structure to store var1 and var2 for each time
struct DataPoint 
{
double time;
double var1;
double var2;
double var3;
};

class DataSet 
{
  public:
    void LoadData(const std::string& filename) 
    {
      std::ifstream file(filename);
      std::string line;
      while (std::getline(file, line)) 
      {
        std::istringstream iss(line);
        DataPoint data;

        // Parse time
        std::string timeStr;
        iss >> timeStr;
        timeStr.erase(std::remove_if(timeStr.begin(), timeStr.end(), [](char c) { return !isdigit(c) && c != '.' && c != 'e' && c != 'E' && c != '-'; }), timeStr.end());
        std::istringstream timeIss(timeStr);
        timeIss >> data.time;

        // Parse var1 and var2
        iss >> data.var1 >> data.var2 >> data.var3;

        m_data.push_back(data);
      }
      file.close();
    }

    // Method to load the first row
    DataPoint GetFirstRow() const 
    {
      if (!m_data.empty()) 
      {
        return m_data.front();
      } 
      else 
      {
        // Return a default DataPoint if the dataset is empty
        return {0.0, 0.0, 0.0, 0.0};
      }
    }

    // // Method to retrieve the row corresponding to a specific time
    // DataPoint GetRow(double time) const 
    // {
    //   auto it = std::find_if(m_data.begin(), m_data.end(), [time](const DataPoint& dp) 
    //   {
    //     return dp.time == time;
    //   });

    //   if (it != m_data.end()) 
    //   {
    //     return *it;
    //   } 
    //   else 
    //   {
    //     // Return a default DataPoint if time is not found
    //     return {0.0, 0.0, 0.0, 0.0};
    //   }
    // }

    DataPoint GetRow(double time) const 
    {
      if (m_data.empty()) 
      {
          return {0.0, 0.0, 0.0}; // Return default if dataset is empty
      }

      // Find the closest time stamp
      auto it = std::min_element(m_data.begin(), m_data.end(), [time](const DataPoint& dp1, const DataPoint& dp2) {
          return std::abs(dp1.time - time) < std::abs(dp2.time - time);
      });

      return *it;
    }


    // Method to load the last row
    DataPoint GetLastRow() const 
    {
      if (!m_data.empty()) 
      {
        return m_data.back();
      } 
      else 
      {
        // Return a default DataPoint if the dataset is empty
        return {0.0, 0.0, 0.0, 0.0};
      }
    }

private:
    std::vector<DataPoint> m_data;
};
///////////////////////////////////////////////////////////////////////////////////////
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

      // destructor
      ~TCStats();

      
      /**
       * \brief Print the statistics.
       * \param os output stream in which the data should be printed.
       */
      void Print(std::ostream& os) const;

      // Member function to reset values
      void reset();
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
     * \brief Get the queueing limit of the current queue for each priority alpha, for FB Algorithm.
     * \param alpha the alpha parameter relevant to the current arriving packet
     * \param alpha_h the pre-determined hyperparameter alpha High
     * \param alpha_l the pre-determined hyperparameter alpha Low
     * \param conjestedQueues
     * \param gamma_i the normalized dequeue rate on port i
     * \returns the maximum number of packets allowed in the queue.
     */
    QueueSize GetQueueThreshold_FB (double_t alpha, double_t alpha_h, double_t alpha_l, int conjestedQueues, float gamma_i);


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
    uint32_t GetNumOfConjestedQueuesInSharedQueue();

    /**
     * \brief Get the current number of conjested (non empty) queues of priority p on the shared buffer, 
     *        in bytes, ifoperating in bytes mode, or packets, otherwise. for a MULTIQUEUE PER PORT architecture.
     * \param queue_priority the priority of the queue that's currently being checked
     * \returns The number of high priority packets in queue disc in bytes or packets.
     */
    uint32_t GetNumOfPriorityConjestedQueuesInSharedQueue(uint32_t queue_priority);

    /**
     * \brief calculate the normalized dequeue rate of queue_i(t). gamma_i(t) 
     *        at the current model, each queue on the port has an equal share of the dequeue BW.
     *        so the actual gamma_i(t) is calculated as: 1/current number of non empty queues on the port.
     * \returns the normalized dequeue rate of queue_i(t).
     */
    float_t GetNormalizedDequeueBandWidth_v1(Ptr<NetDevice> device, uint16_t queueIndex);

    /**
     * \brief calculate the normalized dequeue rate of queue_i_c(t). gamma_i_c(t) 
     *        at the current model, each queue on the port has an equal share of the dequeue BW.
     *        so the actual gamma_i_c(t) is calculated as: 1/current number of non empty queues of priority p (high/low) on the port.
     * \returns the normalized dequeue rate of queue_i_c(t).
     */
    float_t GetNormalizedDequeueBandWidth_v2(Ptr<NetDevice> device, uint8_t flow_riority, uint16_t portIndex, uint16_t queueIndex);

    /**
     * \brief Iterate over all queues and aggrigate the total number of packets enqueued by all of them till this point,
     * \returns the total number of enqueued packets.
     */
    QueueSize GetNumOfTotalEnqueuedPacketsinQueues();

    /**
     * \brief Iterate over all queues and aggrigate the total number of packets of priority P enqueued by all of them till this point,
     * \param queue_priority the priority of the queue that's currently being checked
     * \returns the total number of enqueued packets.
     */
    QueueSize GetNumOfTotalEnqueuedPriorityPacketsinQueues(uint32_t queue_priority);

    /**
     * \brief round some double to one decimal point precission.
     * \returns the 1 decimal precission value of num.
     */
    double_t RoundToOneDecimal(double_t num);
    
    /**
     * \brief calculate the new local mouse/elephant probability d from the predicted traffic in the time interval t: t+Tau.
     * \returns the upcomming d value.
     */
    double_t EstimateNewLocalD();

    /**
     * \brief returns the optimal Alpha High and Low values based on the D of the traffic.
     * \param mice_elephant_prob_val the mice/elephant probability (D) assigned at the OnOff Application
     * \param device the NetDevice that's currently being used as Tx Port.
     * \returns return the new alpha_high and alpha_low value based on the optimization done prior.
     */
    std::pair<double_t, double_t> GetNewAlphaHighAndLow(Ptr<NetDevice> device, uint32_t mice_elephant_prob_val);
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
    std::pair<double_t, double_t> alphas; //!< a new pair of Alpha High/Low assigned by the algorythm
    bool m_adjustableAlphas; // !< True if apply optimal Alpha High/Low values based on current "D"
    // FB queue disc parameters:
    float_t m_gamma;  //!< Normalized de-queue rate per port/queue in a single FIFO per port scenario.
    uint8_t m_nonEmpty; //!< number of non empty queues on each port.
    // Predictive queue disc parameters:
    double_t m_predicted_mice_elephant_prob;  //!< the predicted d value of the upcomming traffic returned by the function
    double_t m_estimated_mice_elephant_prob;  //!< the estimated d value of the traffic returned by the function
    double_t predictedMiceElephantProbVal;  //!< the predicted d value of the upcomming traffic
    uint32_t totalEnqueuedPacketsInQueueCounter = 0;
    uint32_t totalEnqueuedBytesInQueueCounter = 0;
    double_t m_predictedArrivingTraffic; //!< the total predicted number of packets arriving to shared buffer in time interval t - Tau/2: t + Tau/2
    double_t m_predictedArrivingHighPriorityTraffic; //!< the predicted number of user defined Priority packets arriving to shared buffer in time interval t - Tau/2: t + Tau/2
    double_t m_nTotalEnqueuedPacketsInSharedBuffer; //!< the total number of packets enqueued on shared buffer from the beginning of the simulation
    double_t m_passedTraffic; //!< the total number of packets passed through shared buffer during time interval t-Tau: t
    double_t m_passedPriorityTraffic; //!< the total number of user defined Priority packets passed through shared buffer in time interval t-Tau: t
    // uint8_t queue_priority;   //< the priority of the queue that's currently being checked
    uint32_t m_nConjestedQueues;   //!< number of queues that are conjested at current time instance
    uint32_t m_nConjestedQueues_p;   //!< number of queues of priority p that are conjested at current time instance
    float_t m_numConjestedQueues;  //!< total number of queues that are conjested at current time instance
    float_t m_numConjestedQueuesHigh;  //!< number of queues that are conjested at current time instance
    float_t m_numConjestedQueuesLow;  //!< number of queues that are conjested at current time instance
    float_t numOfClasses;  //!< total number of classes. for now it's only High Priority/ Low Priority
    // Flow Classification Parameters
    double_t m_Tau; //!< the length of the winddow in time to monitor incoming traffic, to estimate local d 
    double_t m_Num_M_High;  //!< the number of OnOff machines that generate High Priority traffic
    MiceElephantProbabilityTag miceElephantProbTag;  //!< a Tag that represents the mice/elephant probability assigned to the flow by the user
    double_t m_miceElephantProbValFromTag;  //!< the d value of the flow that was assigned at the OnOff application
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
