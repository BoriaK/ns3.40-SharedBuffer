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

#include "traffic-control-layer.h"

#include "queue-disc.h"

#include "ns3/log.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/object-map.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
////////////////////////////////
#include "ns3/names.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/queue.h"
#include "ns3/string.h"
//////////////////////////////

#include <iostream>
#include <cstdlib>
#include <string>
#include <tuple>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TrafficControlLayer");

NS_OBJECT_ENSURE_REGISTERED(TrafficControlLayer);

ATTRIBUTE_HELPER_CPP(TcPriomap);

TypeId
TrafficControlLayer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TrafficControlLayer")
            .SetParent<Object>()
            .SetGroupName("TrafficControl")
            .AddConstructor<TrafficControlLayer>()
            .AddAttribute(
                "RootQueueDiscList",
                "The list of root queue discs associated to this Traffic Control layer.",
                ObjectMapValue(),
                MakeObjectMapAccessor(&TrafficControlLayer::GetNDevices,
                                      &TrafficControlLayer::GetRootQueueDiscOnDeviceByIndex),
                MakeObjectMapChecker<QueueDisc>())
            /////Added by me////////////////////////////////////////////////////////////////////
            .AddAttribute(
                "SharedBuffer",
                "True to use Shared-Buffer all packet flows are managed before the queue-disc",
                BooleanValue(false),
                MakeBooleanAccessor(&TrafficControlLayer::m_useSharedBuffer),
                MakeBooleanChecker())
            .AddAttribute("MaxSharedBufferSize",
                          "in case of Shared-Buffer only, The maximum number of packets accepted "
                          "by traffic-controll layer.",
                          QueueSizeValue(QueueSize("100p")),
                          MakeQueueSizeAccessor(&TrafficControlLayer::m_maxSharedBufferSize),
                          MakeQueueSizeChecker())
            .AddAttribute("Alpha_High",
                          "The Alpha value for high priority packets",
                          DoubleValue(2),
                          MakeDoubleAccessor(&TrafficControlLayer::m_alpha_h),
                          MakeDoubleChecker<double_t>())
            .AddAttribute("Alpha_Low",
                          "The Alpha value for low priority packets",
                          DoubleValue(1),
                          MakeDoubleAccessor(&TrafficControlLayer::m_alpha_l),
                          MakeDoubleChecker<double_t>())
            .AddAttribute("NumberOfHighProbabilityOnOffMachines",
                          "The number of OnOff machines that generate high priority traffic set by the user for the simulation",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&TrafficControlLayer::m_Num_M_High),
                          MakeDoubleChecker<double_t>())
            .AddAttribute("AdjustableAlphas",
                          "To adjust Alpha High/Low to optimal values based on estimated 'D' value",
                          BooleanValue(false),
                          MakeBooleanAccessor(&TrafficControlLayer::m_adjustableAlphas),
                          MakeBooleanChecker())
            .AddAttribute("TrafficEstimationWindowLength",
                        "The length of the window to monitor incomming (passed and future) traffic to estimate local d value",
                        DoubleValue(0.2),
                        MakeDoubleAccessor(&TrafficControlLayer::m_Tau),
                        MakeDoubleChecker<double_t>())
            .AddAttribute(
                "TrafficControllAlgorythm",
                "The Traffic Controll Algorythm to use inorder to manage traffic in Shared Buffer",
                StringValue("FB"),
                MakeStringAccessor(&TrafficControlLayer::m_usedAlgorythm),
                MakeStringChecker())
            .AddAttribute(
                "PriorityMapforMultiQueue",
                "The Priority Map that's used in the Round Robbin algorythm for each port in "
                "MultiQueue scenario",
                TcPriomapValue(TcPriomap{{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}),
                MakeTcPriomapAccessor(&TrafficControlLayer::m_tosPrioMap),
                MakeTcPriomapChecker())
            .AddTraceSource(
                "PacketsInQueue",
                "Number of packets currently stored in the Shared Buffer in Traffic Control Layer",
                MakeTraceSourceAccessor(&TrafficControlLayer::m_traceSharedBufferPackets),
                "ns3::TracedValueCallback::Uint32")
            .AddTraceSource(
                "BytesInQueue",
                "Number of bytes currently stored in the Shared Buffer in Traffic Control Layer",
                MakeTraceSourceAccessor(&TrafficControlLayer::m_traceSharedBufferBytes),
                "ns3::TracedValueCallback::Uint32")
            .AddTraceSource(
                "HighPriorityPacketsInQueue",
                "Number of packets currently stored in the Shared Buffer in Traffic Control Layer",
                MakeTraceSourceAccessor(&TrafficControlLayer::m_nPackets_trace_High_InSharedQueue),
                "ns3::TracedValueCallback::Uint32")
            .AddTraceSource(
                "LowPriorityPacketsInQueue",
                "Number of packets currently stored in the Shared Buffer in Traffic Control Layer",
                MakeTraceSourceAccessor(&TrafficControlLayer::m_nPackets_trace_Low_InSharedQueue),
                "ns3::TracedValueCallback::Uint32")
            .AddTraceSource("EnqueueingThreshold_High_0",
                            "The Threshold for High Priority packets in the Shared Buffer in "
                            "Traffic Control Layer",
                            MakeTraceSourceAccessor(&TrafficControlLayer::m_p_trace_threshold_h_0),
                            "ns3::TracedValueCallback::Uint32")
            .AddTraceSource("EnqueueingThreshold_High_1",
                            "The Threshold for High Priority packets in the Shared Buffer in "
                            "Traffic Control Layer",
                            MakeTraceSourceAccessor(&TrafficControlLayer::m_p_trace_threshold_h_1),
                            "ns3::TracedValueCallback::Uint32")
            .AddTraceSource("EnqueueingThreshold_Low_0",
                            "The Threshold for Low Priority packets in the Shared Buffer in "
                            "Traffic Control Layer",
                            MakeTraceSourceAccessor(&TrafficControlLayer::m_p_trace_threshold_l_0),
                            "ns3::TracedValueCallback::Uint32")
            .AddTraceSource("EnqueueingThreshold_Low_1",
                            "The Threshold for Low Priority packets in the Shared Buffer in "
                            "Traffic Control Layer",
                            MakeTraceSourceAccessor(&TrafficControlLayer::m_p_trace_threshold_l_1),
                            "ns3::TracedValueCallback::Uint32")
            //////////////////////////////////////////////////////////////////////////////////////////////
            .AddTraceSource("TcDrop",
                            "Trace source indicating a packet has been dropped by the Traffic "
                            "Control layer because no queue disc is installed on the device, the "
                            "device supports flow control and the device queue is stopped",
                            MakeTraceSourceAccessor(&TrafficControlLayer::m_dropped),
                            "ns3::Packet::TracedCallback");
    return tid;
}

TypeId
TrafficControlLayer::GetInstanceTypeId() const
{
    return GetTypeId();
}

TrafficControlLayer::TrafficControlLayer()
    : Object()
{
    NS_LOG_FUNCTION(this);
}

TrafficControlLayer::~TrafficControlLayer()
{
    NS_LOG_FUNCTION(this);
}

void
TrafficControlLayer::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_node = nullptr;
    m_handlers.clear();
    m_netDevices.clear();
    Object::DoDispose();
}

void
TrafficControlLayer::DoInitialize()
{
    NS_LOG_FUNCTION(this);

    ScanDevices();

    // initialize the root queue discs
    for (auto& ndi : m_netDevices)
    {
        if (ndi.second.m_rootQueueDisc)
        {
            ndi.second.m_rootQueueDisc->Initialize();
        }
    }

    Object::DoInitialize();
}

void
TrafficControlLayer::RegisterProtocolHandler(Node::ProtocolHandler handler,
                                             uint16_t protocolType,
                                             Ptr<NetDevice> device)
{
    NS_LOG_FUNCTION(this << protocolType << device);

    ProtocolHandlerEntry entry;
    entry.handler = handler;
    entry.protocol = protocolType;
    entry.device = device;
    entry.promiscuous = false;

    m_handlers.push_back(entry);

    NS_LOG_DEBUG("Handler for NetDevice: " << device << " registered for protocol " << protocolType
                                           << ".");
}

//////////added by me/////////////////////////////////////
TrafficControlLayer::TCStats::TCStats()
    : nTotalDroppedPackets(0),
      nTotalDroppedPacketsHighPriority(0), // added by me
      nTotalDroppedPacketsLowPriority(0),  // added by me
      nTotalDroppedBytes(0),
      nTotalDroppedBytesHighPriority(0), // added by me
      nTotalDroppedBytesLowPriority(0)   // added by me
{
}

TrafficControlLayer::TCStats::~TCStats()
{
    NS_LOG_FUNCTION(this);
}

// Member function implementation to reset values
void
TrafficControlLayer::TCStats::reset()
{
    nTotalDroppedPackets = 0;
    nTotalDroppedPacketsHighPriority = 0;
    nTotalDroppedPacketsLowPriority = 0;
    nTotalDroppedBytes = 0;
    nTotalDroppedBytesHighPriority = 0;
    nTotalDroppedBytesLowPriority = 0;
}

void
TrafficControlLayer::TCStats::Print(std::ostream& os) const
{
    std::map<std::string, uint32_t>::const_iterator itp;
    std::map<std::string, uint64_t>::const_iterator itb;

    os << std::endl
       << "Packets/Bytes dropped by Traffic Control Layer: " << nTotalDroppedPackets << " / "
       << nTotalDroppedBytes
       /////////Added by me//////////////////////
       << std::endl
       << "High Priority Packets/Bytes dropped by Traffic Control Layer: "
       << nTotalDroppedPacketsHighPriority << " / " << nTotalDroppedBytesHighPriority << std::endl
       << "Low Priority Packets/Bytes dropped by Traffic Control Layer: "
       << nTotalDroppedPacketsLowPriority << " / " << nTotalDroppedBytesLowPriority;
    /////////////////////////////////////////

    os << std::endl;
}

const TrafficControlLayer::TCStats&
TrafficControlLayer::GetStats()
{
    return m_stats;
}

std::ostream&
operator<<(std::ostream& os, const TrafficControlLayer::TCStats& stats)
{
    stats.Print(os);
    return os;
}

size_t
TrafficControlLayer::GetNetDeviceIndex(Ptr<NetDevice> device)
{
    int index = -1;
    int currentIndex = 0;

    for (const auto& pair : m_netDevices)
    {
        if (pair.first == device)
        {
            index = currentIndex;
            break;
        }
        currentIndex++;
    }

    // std::cout << "Index of '" << device << "': " << index << std::endl;

    return index;
}

QueueSize
TrafficControlLayer::GetMaxSharedBufferSize() const
{
    NS_LOG_FUNCTION(this);

    return m_maxSharedBufferSize;
}

QueueSize
TrafficControlLayer::GetCurrentSharedBufferSize()
{
    NS_LOG_FUNCTION(this);

    /// Create a vector to hold pointers to all the NetDevices aggrigated to the Node
    std::vector<Ptr<NetDevice>>
        m_netDevicesList; // a List containing all the net devices on a speciffic Node
    for (const auto& element : m_netDevices)
    {
        m_netDevicesList.push_back(element.first);
    }

    // for debug: Get the number of NetDevices installed on the Node
    // uint32_t numDevices = m_netDevicesList.size();
    // std::cout << "Number of NetDevices installed on the Node is: " << numDevices << std::endl;

    // calculate the number of the packets in the Shared Buffer as the sum of all packets currently
    // stored on all the NetDevices on the Node: erase the Shared Buffer Packets calcultaion from
    // the previous itteration
    m_sharedBufferPackets = 0;
    m_sharedBufferBytes = 0;

    /// calculate the number of the packets/bytes in the Shared Buffer
    // start the index from 2 since the first 2 net devices are for Tx Ack packets back to senders
    // in TCP scenario
    Ptr<NetDeviceQueueInterface> devQueueIface;  // not sure what this is for ??
    for (size_t i = 0; i < m_netDevicesList.size(); i++)
    {
        Ptr<NetDevice> ndev = m_netDevicesList[i];
        std::string netDeviceName = Names::FindName(ndev); // Get the name of the net-device
        if (netDeviceName.find("switchDeviceOut")!=std::string::npos)
        {
            std::map<Ptr<NetDevice>, NetDeviceInfo>::iterator ndi = m_netDevices.find(ndev);
            if (ndi == m_netDevices.end() || !ndi->second.m_rootQueueDisc) // if no root-queue-disc is installed on the net-device
            {
                Ptr<PointToPointNetDevice> p2pndev = DynamicCast<PointToPointNetDevice>(ndev);
                Ptr<Queue<Packet>> queue = p2pndev->GetQueue();
                trafficControllPacketCounter = queue->GetNPackets();
                trafficControllBytesCounter = queue->GetNBytes();
                m_sharedBufferPackets += trafficControllPacketCounter;
                m_sharedBufferBytes += trafficControllBytesCounter;
            }
            else // if we use a queue-disc on NetDevice as port
            {
                if (ndi->second.m_rootQueueDisc->GetNQueueDiscClasses() > 1) // if we use multi-queue per port
                {
                    for (size_t j = 0; j < ndi->second.m_rootQueueDisc->GetNQueueDiscClasses(); j++)
                    {
                        Ptr<QueueDisc> qDisc =
                            ndi->second.m_rootQueueDisc->GetQueueDiscClass(j)->GetQueueDisc();
                        trafficControllPacketCounter = qDisc->GetNPackets();
                        trafficControllBytesCounter = qDisc->GetNBytes();
                        m_sharedBufferPackets += trafficControllPacketCounter;
                        m_sharedBufferBytes += trafficControllBytesCounter;
                    }
                }
                else
                {
                    Ptr<QueueDisc> qDisc = ndi->second.m_rootQueueDisc; // if we use a single queue per port
                    trafficControllPacketCounter = qDisc->GetNPackets();
                    trafficControllBytesCounter = qDisc->GetNBytes();
                    m_sharedBufferPackets += trafficControllPacketCounter;
                    m_sharedBufferBytes += trafficControllBytesCounter;
                }
            }
        }
    }

    if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::PACKETS)
    {
        return QueueSize(QueueSizeUnit::PACKETS, m_sharedBufferPackets);
    }
    if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::BYTES)
    {
        return QueueSize(QueueSizeUnit::BYTES, m_sharedBufferBytes);
    }
    NS_ABORT_MSG("Unknown queue size unit");
}

QueueSize
TrafficControlLayer::GetNumOfHighPriorityPacketsInSharedQueue()
{
    NS_LOG_FUNCTION(this);

    /// Create a vector to hold pointers to all the NetDevices aggrigated to the Node
    std::vector<Ptr<NetDevice>>
        m_netDevicesList; // a List containing all the net devices on a speciffic Node
    for (const auto& element : m_netDevices)
    {
        m_netDevicesList.push_back(element.first);
    }

    // calculate the number of the packets in the Shared Buffer as the sum of all packets currently
    // stored on all the NetDevices on the Node: erase the Shared Buffer Packets calcultaion from
    // the previous itteration
    m_nPackets_High_InSharedQueue = 0;
    // m_nBytes_high_InSharedQueue = 0; // not implemented for bytes yet

    /// calculate the number of the packets/bytes in the Shared Buffer
    // start the index from 2 since the first 2 net devices are for Tx Ack packets back to senders
    // in TCP scenario
    for (size_t i = 0; i < m_netDevicesList.size(); i++)
    {
        Ptr<NetDevice> ndev = m_netDevicesList[i];
        std::string netDeviceName = Names::FindName(ndev); // Get the name of the net-device
        if (netDeviceName.find("switchDeviceOut")!=std::string::npos)
        {
            std::map<Ptr<NetDevice>, NetDeviceInfo>::iterator ndi = m_netDevices.find(ndev);
            if (ndi == m_netDevices.end() || !ndi->second.m_rootQueueDisc) // if no root-queue-disc is installed on the net-device
            {
                Ptr<PointToPointNetDevice> p2pndev = DynamicCast<PointToPointNetDevice>(ndev);
                Ptr<Queue<Packet>> queue = p2pndev->GetQueue();
                trafficControllPacketCounter = queue->GetNumOfHighPrioPacketsInQueue().GetValue();
                // trafficControllBytesCounter = queue->GetNumOfHighPrioPacketsInQueue().GetValue();  //
                // not implemented for bytes yet
                m_nPackets_High_InSharedQueue += trafficControllPacketCounter;
                // m_nBytes_h_InSharedQueue += trafficControllBytesCounter;  // not implemented for
                // bytes yet
            }
            else // if we use a queue-disc on NetDevice as port
            {
                if (ndi->second.m_rootQueueDisc->GetNQueueDiscClasses() > 1) // if we use multiple queues/port
                {
                    for (size_t j = 0; j < ndi->second.m_rootQueueDisc->GetNQueueDiscClasses(); j++)
                    {
                        Ptr<QueueDisc> qDisc =
                            ndi->second.m_rootQueueDisc->GetQueueDiscClass(j)->GetQueueDisc();
                        trafficControllPacketCounter =
                            qDisc->GetNumOfHighPrioPacketsInQueue().GetValue();
                        // trafficControllBytesCounter =
                        // qDisc->GetNumOfHighPrioPacketsInQueue().GetValue();  // not implemented for
                        // bytes yet
                        m_nPackets_High_InSharedQueue += trafficControllPacketCounter;
                        // m_nBytes_h_InSharedQueue += trafficControllBytesCounter;  // not implemented
                        // for bytes yet
                    }
                }
                else // if we use single queue/port
                {
                    Ptr<QueueDisc> qDisc = ndi->second.m_rootQueueDisc;
                    trafficControllPacketCounter = qDisc->GetNumOfHighPrioPacketsInQueue().GetValue();
                    // trafficControllBytesCounter = qDisc->GetNumOfHighPrioPacketsInQueue().GetValue();
                    // // not implemented for bytes yet
                    m_nPackets_High_InSharedQueue += trafficControllPacketCounter;
                    // m_nBytes_h_InSharedQueue += trafficControllBytesCounter;  // not implemented for
                    // bytes yet
                }
            }
        }
    }

    if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::PACKETS)
    {
        return QueueSize(QueueSizeUnit::PACKETS, m_nPackets_High_InSharedQueue);
    }
    // if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::BYTES)  // not implemented for bytes
    // yet
    // {
    //     return QueueSize(QueueSizeUnit::BYTES, m_nBytes_h_InSharedQueue);
    // }
    NS_ABORT_MSG("Unknown queue size unit");
}

QueueSize
TrafficControlLayer::GetNumOfLowPriorityPacketsInSharedQueue()
{
    NS_LOG_FUNCTION(this);

    /// Create a vector to hold pointers to all the NetDevices aggrigated to the Node
    std::vector<Ptr<NetDevice>>
        m_netDevicesList; // a List containing all the net devices on a speciffic Node
    for (const auto& element : m_netDevices)
    {
        m_netDevicesList.push_back(element.first);
    }

    // calculate the number of the packets in the Shared Buffer as the sum of all packets currently
    // stored on all the NetDevices on the Node: erase the Shared Buffer Packets calcultaion from
    // the previous itteration
    m_nPackets_Low_InSharedQueue = 0;
    // m_nBytes_low_InSharedQueue = 0;  // not implemented for bytes yet

    /// calculate the number of the packets/bytes in the Shared Buffer
    // start the index from 2 since the first 2 net devices are for Tx Ack packets back to senders
    // in TCP scenario
    for (size_t i = 0; i < m_netDevicesList.size(); i++)
    {
        Ptr<NetDevice> ndev = m_netDevicesList[i];
        std::string netDeviceName = Names::FindName(ndev); // Get the name of the net-device
        if (netDeviceName.find("switchDeviceOut")!=std::string::npos)
        {
            std::map<Ptr<NetDevice>, NetDeviceInfo>::iterator ndi = m_netDevices.find(ndev);
            if (ndi == m_netDevices.end() || !ndi->second.m_rootQueueDisc) // if no root-queue-disc is installed on the net-device
            {
                Ptr<PointToPointNetDevice> p2pndev = DynamicCast<PointToPointNetDevice>(ndev);
                Ptr<Queue<Packet>> queue = p2pndev->GetQueue();
                trafficControllPacketCounter = queue->GetNumOfLowPrioPacketsInQueue().GetValue();
                // trafficControllBytesCounter = queue->GetNumOfLowPrioPacketsInQueue().GetValue();  //
                // not implemented for bytes yet
                m_nPackets_Low_InSharedQueue += trafficControllPacketCounter;
                // m_nBytes_l_InSharedQueue += trafficControllBytesCounter;  // not implemented for
                // bytes yet
            }
            else // if we use a queue-disc on NetDevice as port
            {
                if (ndi->second.m_rootQueueDisc->GetNQueueDiscClasses() > 1) // if we use multiple queues/port
                {
                    for (size_t j = 0; j < ndi->second.m_rootQueueDisc->GetNQueueDiscClasses(); j++)
                    {
                        Ptr<QueueDisc> qDisc = ndi->second.m_rootQueueDisc->GetQueueDiscClass(j)->GetQueueDisc();
                        trafficControllPacketCounter = qDisc->GetNumOfLowPrioPacketsInQueue().GetValue();
                        // trafficControllBytesCounter =
                        // qDisc->GetNumOfLowPrioPacketsInQueue().GetValue();  // not implemented for
                        // bytes yet
                        m_nPackets_Low_InSharedQueue += trafficControllPacketCounter;
                        // m_nBytes_l_InSharedQueue += trafficControllBytesCounter;  // not implemented
                        // for bytes yet
                    }
                }
                else // if we use single queue/port
                {
                    Ptr<QueueDisc> qDisc = ndi->second.m_rootQueueDisc;
                    trafficControllPacketCounter = qDisc->GetNumOfLowPrioPacketsInQueue().GetValue();
                    // trafficControllBytesCounter = qDisc->GetNumOfLowPrioPacketsInQueue().GetValue();
                    // // not implemented for bytes yet
                    m_nPackets_Low_InSharedQueue += trafficControllPacketCounter;
                    // m_nBytes_l_InSharedQueue += trafficControllBytesCounter;  // not implemented for
                    // bytes yet
                }
            }
        }
    }
    if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::PACKETS)
    {
        return QueueSize(QueueSizeUnit::PACKETS, m_nPackets_Low_InSharedQueue);
    }
    // if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::BYTES)  // not implemented for bytes
    // yet
    // {
    //     return QueueSize(QueueSizeUnit::BYTES, m_nBytes_low_InSharedQueue);
    // }
    NS_ABORT_MSG("Unknown queue size unit");
}

uint32_t
TrafficControlLayer::GetNumOfConjestedQueuesInSharedQueue() // this version referres to conjested as "non-empty"
{
    NS_LOG_FUNCTION(this);

    /// Create a vector to hold pointers to all the NetDevices aggrigated to the Node
    std::vector<Ptr<NetDevice>>
        m_netDevicesList; // a List containing all the net devices on a speciffic Node
    for (const auto& element : m_netDevices)
    {
        m_netDevicesList.push_back(element.first);
    }

    // calculate the number of the conjested queues in the Shared Buffer as the sum of all non empty
    // queues of priority p on all the NetDevices on the Node: erase the conjested queues
    // calcultaion from the previous itteration
    m_nConjestedQueues = 0;

    /// calculate the number of the qunjested queues of priority p in the Shared Buffer
    // start the index from 2 since the first 2 net devices are for Tx Ack packets back to senders
    // in TCP scenario
    for (size_t i = 0; i < m_netDevicesList.size(); i++)
    {
        Ptr<NetDevice> ndev = m_netDevicesList[i];
        std::string netDeviceName = Names::FindName(ndev); // Get the name of the net-device
        if (netDeviceName.find("switchDeviceOut")!=std::string::npos)
        {
            std::map<Ptr<NetDevice>, NetDeviceInfo>::iterator ndi = m_netDevices.find(ndev);
            if (ndi == m_netDevices.end() || !ndi->second.m_rootQueueDisc) // if no root-queue-disc is installed on the net-device
            {
                Ptr<PointToPointNetDevice> p2pndev = DynamicCast<PointToPointNetDevice>(ndev);
                Ptr<Queue<Packet>> queue = p2pndev->GetQueue();
                // if (queue->GetNPackets())
                if (queue->GetNPackets())
                {
                    m_nConjestedQueues++;
                }
            }
            else // if we use a queue-disc on NetDevice as port
            {
                if (ndi->second.m_rootQueueDisc->GetNQueueDiscClasses() > 1) // if we use multiple queues/port
                {
                    for (size_t j = 0; j < ndi->second.m_rootQueueDisc->GetNQueueDiscClasses(); j++)
                    {
                        Ptr<QueueDisc> qDisc =
                            ndi->second.m_rootQueueDisc->GetQueueDiscClass(j)->GetQueueDisc();
                        if (qDisc->GetNPackets())
                        {
                            m_nConjestedQueues++;
                        }
                    }
                }
                else // if we use single queue/port
                {
                    Ptr<QueueDisc> qDisc = ndi->second.m_rootQueueDisc;
                    if (qDisc->GetNPackets())
                    {
                        m_nConjestedQueues++;
                    }
                }
            }
        }
    }
    return m_nConjestedQueues;
}

uint32_t
TrafficControlLayer::GetNumOfPriorityConjestedQueuesInSharedQueue(uint32_t queue_priority) // this version referres to conjested as "non-empty"
{
    NS_LOG_FUNCTION(this);

    /// Create a vector to hold pointers to all the NetDevices aggrigated to the Node
    std::vector<Ptr<NetDevice>>
        m_netDevicesList; // a List containing all the net devices on a speciffic Node
    for (const auto& element : m_netDevices)
    {
        m_netDevicesList.push_back(element.first);
    }

    // calculate the number of the conjested queues in the Shared Buffer as the sum of all non empty
    // queues of priority p on all the NetDevices on the Node: erase the conjested queues
    // calcultaion from the previous itteration
    m_nConjestedQueues_p = 0;

    /// calculate the number of the qunjested queues of priority p in the Shared Buffer
    for (size_t i = 0; i < m_netDevicesList.size(); i++)  // itterate over all ports
    {
        Ptr<NetDevice> ndev = m_netDevicesList[i];
        std::string netDeviceName = Names::FindName(ndev); // Get the name of the net-device
        if (netDeviceName.find("switchDeviceOut")!=std::string::npos)
        {
            std::map<Ptr<NetDevice>, NetDeviceInfo>::iterator ndi = m_netDevices.find(ndev);
            if (ndi == m_netDevices.end() || !ndi->second.m_rootQueueDisc) // if no root-queue-disc is installed on the net-device
            {
                Ptr<PointToPointNetDevice> p2pndev = DynamicCast<PointToPointNetDevice>(ndev);
                Ptr<Queue<Packet>> queue = p2pndev->GetQueue();
                if (queue_priority == 1) // 1 is high priority
                {
                    if (queue->GetNumOfHighPrioPacketsInQueue().GetValue())
                    {
                        m_nConjestedQueues_p++;
                        return m_nConjestedQueues_p; // because there is no scenario in which there's 2 ports of the same priority
                    }
                }
                else if (queue_priority == 2) // 2 is low priority
                {
                    if (queue->GetNumOfLowPrioPacketsInQueue().GetValue())
                    {
                        m_nConjestedQueues_p++;
                        return m_nConjestedQueues_p; // because there is no scenario in which there's 2 ports of the same priority
                    }
                }
                else
                {
                    NS_ABORT_MSG("Unknown priority");
                }
            }
            else // if we use a queue-disc on NetDevice as port
            {
                if (ndi->second.m_rootQueueDisc->GetNQueueDiscClasses() > 1) // if we use multiple queues/port
                {
                    for (size_t j = 0; j < ndi->second.m_rootQueueDisc->GetNQueueDiscClasses(); j++)
                    {
                        Ptr<QueueDisc> qDisc =
                            ndi->second.m_rootQueueDisc->GetQueueDiscClass(j)->GetQueueDisc();
                        if (queue_priority == 1) // 1 is high priority
                        {
                            if (qDisc->GetNumOfHighPrioPacketsInQueue().GetValue())
                            {
                                m_nConjestedQueues_p++;
                            }
                        }
                        else if (queue_priority == 2) // 2 is low priority
                        {
                            if (qDisc->GetNumOfLowPrioPacketsInQueue().GetValue())
                            {
                                m_nConjestedQueues_p++;
                            }
                        }
                        else
                        {
                            NS_ABORT_MSG("Unknown priority");
                        }
                    }
                }
                else // if we use single queue/port
                {
                    Ptr<QueueDisc> qDisc = ndi->second.m_rootQueueDisc;
                    if (queue_priority == 1) // 1 is high priority
                    {
                        if (qDisc->GetNumOfHighPrioPacketsInQueue().GetValue())
                        {
                            m_nConjestedQueues_p++;
                            return m_nConjestedQueues_p; // because there is no scenario in which there's 2 ports of the same priority
                        }
                    }
                    else if (queue_priority == 2) // 2 is low priority
                    {
                        if (qDisc->GetNumOfLowPrioPacketsInQueue().GetValue())
                        {
                            m_nConjestedQueues_p++;
                            return m_nConjestedQueues_p; // because there is no scenario in which there's 2 ports of the same priority
                        }
                    }
                    else
                    {
                        NS_ABORT_MSG("Unknown priority");
                    }
                }
            }
        }
    }
    return m_nConjestedQueues_p;
}

float_t
TrafficControlLayer::GetNormalizedDequeueBandWidth_v1(Ptr<NetDevice> device, uint16_t queueIndex)
{
    NS_LOG_FUNCTION(this);

    // this version DOESN'T ASSIGN different dequeue rates for packets of different class from the same port.

    // initilize non empty queues/port count at each itteration
    m_nonEmpty = 0;

    Ptr<NetDevice> ndev = device;
    std::map<Ptr<NetDevice>, NetDeviceInfo>::iterator ndi = m_netDevices.find(ndev);
    if (ndi == m_netDevices.end() ||
        !ndi->second.m_rootQueueDisc) // if no root-queue-disc is installed on the net-device
    {
        m_nonEmpty = 1; // gamma_i(t) is always 1 when there's a single queue/port scenario
    }
    else // if we use a queue-disc on NetDevice as port
    {
        if (ndi->second.m_rootQueueDisc->GetNQueueDiscClasses() == 0) // if we use single queue/port
        {
            m_nonEmpty = 1;
        }
        else // if we use multiple queues/port
        {
            for (size_t j = 0; j < ndi->second.m_rootQueueDisc->GetNQueueDiscClasses(); j++)
            {
                Ptr<QueueDisc> qDisc = ndi->second.m_rootQueueDisc->GetQueueDiscClass(j)->GetQueueDisc();
                // if queue_i(t) is either non empty or it's the same queue_i(t) that's about to
                // recieve the next packet
                if (qDisc->GetNPackets() || j == queueIndex)
                {
                    m_nonEmpty++;
                }
            }
        }
    }
    // calculate the normalized dequeue rate of a queue on a port (net-device) as 1/non-empty queues
    // on this port
    m_gamma = 1.0 / m_nonEmpty;
    return m_gamma;
}

float_t
TrafficControlLayer::GetNormalizedDequeueBandWidth_v2(Ptr<NetDevice> device, uint8_t flow_priority, uint16_t portIndex, uint16_t queueIndex)
{
    NS_LOG_FUNCTION(this);

    // this version ASSIGNES different dequeue rates for packets of different class from the same port.
    
    // initilize non empty queues/port count at each itteration
    m_nonEmpty = 0;

    Ptr<NetDevice> ndev = device;
    std::map<Ptr<NetDevice>, NetDeviceInfo>::iterator ndi = m_netDevices.find(ndev);
    if (ndi == m_netDevices.end() ||
        !ndi->second.m_rootQueueDisc) // if no root-queue-disc is installed on the net-device
    {
        m_nonEmpty = 1; // gamma_i(t) is always 1 when there's a single queue/port scenario
    }
    else // if we use a queue-disc on NetDevice as port
    {
        if (ndi->second.m_rootQueueDisc->GetNQueueDiscClasses() == 0) // if we use single queue/port
        {
            m_nonEmpty = 1;
        }
        else // if we use multiple queues/port
        {
            // esteblish which sub-queues are High/Low priority for each port from the D value assigned by the user for the simulation:
            std::cout << "the number of OnOff machines that generate High Priority traffic: " << m_Num_M_High << std::endl;
            if (flow_priority == 1)  // for High Priority packets, go over all high priority sub queues and check
            {
                for (size_t j = 0; j < ndi->second.m_rootQueueDisc->GetNQueueDiscClasses(); j++)
                {
                    Ptr<QueueDisc> qDisc =
                        ndi->second.m_rootQueueDisc->GetQueueDiscClass(j)->GetQueueDisc();
                    if (portIndex == 0 && j < int(ceil(m_Num_M_High/2)) || portIndex == 1 && j < int(floor(m_Num_M_High/2)))
                    {
                        // if queue_i(t) is either non empty or it's the same queue_i(t) that's about to
                        // recieve the next packet
                        if (qDisc->GetNPackets() || j == queueIndex)
                        {
                            m_nonEmpty++;
                        }
                    }
                }
            }
            else if (flow_priority == 2)  // for low priority packets, go over all low priority sub queues and check
            {
                for (size_t j = 0; j < ndi->second.m_rootQueueDisc->GetNQueueDiscClasses(); j++)
                {
                    Ptr<QueueDisc> qDisc =
                        ndi->second.m_rootQueueDisc->GetQueueDiscClass(j)->GetQueueDisc();
                    if (portIndex == 0 && j >= int(ceil(m_Num_M_High/2)) || portIndex == 1 && j >= int(floor(m_Num_M_High/2)))
                    {
                        // if queue_i(t) is either non empty or it's the same queue_i(t) that's about to
                        // recieve the next packet
                        if (qDisc->GetNPackets() || j == queueIndex)
                        {
                            m_nonEmpty++;
                        }
                    }
                }
            }
        }
    }
    // calculate the normalized dequeue rate of a queue on a port (net-device) as 1/non-empty queues
    // on this port
    m_gamma = 1.0 / m_nonEmpty;
    return m_gamma;
}

QueueSize
TrafficControlLayer::GetQueueThreshold_DT(double_t alpha, double_t alpha_l, double_t alpha_h) // added by me!!!!!!!! for DT implementation
{
    NS_LOG_FUNCTION(this);

    if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::PACKETS)
    {
        if (alpha == alpha_h)
        {
            m_p_threshold_h = alpha_h * (GetMaxSharedBufferSize().GetValue() -
                                         GetCurrentSharedBufferSize().GetValue());
            return QueueSize(QueueSizeUnit::PACKETS, m_p_threshold_h);
        }
        else if (alpha == alpha_l)
        {
            m_p_threshold_l = alpha_l * (GetMaxSharedBufferSize().GetValue() -
                                         GetCurrentSharedBufferSize().GetValue());
            return QueueSize(QueueSizeUnit::PACKETS, m_p_threshold_l);
        }
    }
    if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::BYTES)
    {
        if (alpha == alpha_h)
        {
            m_b_threshold_h = alpha_h * (GetMaxSharedBufferSize().GetValue() -
                                         GetCurrentSharedBufferSize().GetValue());
            return QueueSize(QueueSizeUnit::PACKETS, m_b_threshold_h);
        }
        else if (alpha == alpha_l)
        {
            m_p_threshold_l = alpha_l * (GetMaxSharedBufferSize().GetValue() -
                                         GetCurrentSharedBufferSize().GetValue());
            return QueueSize(QueueSizeUnit::PACKETS, m_b_threshold_l);
        }
    }
    NS_ABORT_MSG("Unknown Threshod unit");
}

QueueSize
TrafficControlLayer::GetQueueThreshold_FB(double_t alpha, double_t alpha_l, double_t alpha_h, int conjestedQueues, float gamma) // for FB implementation. v2 excepts Np(t) as an external variable
{
    NS_LOG_FUNCTION(this);

    m_gamma = gamma;

    m_numConjestedQueuesHigh = 0; // initilize to 0 EACH TIME A NEW PACKET ARRIVES
    m_numConjestedQueuesLow = 0;

    if (conjestedQueues ==
        0) // if queue is compleatly empty -> Threshold should be as high as possible (Infinity)
    {
        if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::PACKETS)
        {
            if (alpha == alpha_h)
            {
                m_p_threshold_h =
                    alpha_h * m_gamma *
                    (GetMaxSharedBufferSize().GetValue() - GetCurrentSharedBufferSize().GetValue());
                return QueueSize(QueueSizeUnit::PACKETS, m_p_threshold_h);
            }
            else if (alpha == alpha_l)
            {
                m_p_threshold_l =
                    alpha_l * m_gamma *
                    (GetMaxSharedBufferSize().GetValue() - GetCurrentSharedBufferSize().GetValue());
                return QueueSize(QueueSizeUnit::PACKETS, m_p_threshold_l);
            }
        }
        if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::BYTES)
        {
            if (alpha == alpha_h)
            {
                m_b_threshold_h =
                    alpha_h * m_gamma *
                    (GetMaxSharedBufferSize().GetValue() - GetCurrentSharedBufferSize().GetValue());
                return QueueSize(QueueSizeUnit::PACKETS, m_b_threshold_h);
            }
            else if (alpha == alpha_l)
            {
                m_p_threshold_l =
                    alpha_l * m_gamma *
                    (GetMaxSharedBufferSize().GetValue() - GetCurrentSharedBufferSize().GetValue());
                return QueueSize(QueueSizeUnit::PACKETS, m_b_threshold_l);
            }
        }
        NS_ABORT_MSG("Unknown Threshod unit");
    }
    else // some queues are already congested
    {
        m_numConjestedQueues = conjestedQueues;
        if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::PACKETS)
        {
            // queue_priority = [2, 1]
            if (alpha == alpha_h)
            {
                m_numConjestedQueuesHigh = conjestedQueues;
                m_p_threshold_h =
                    alpha_h * (1 / m_numConjestedQueuesHigh) * m_gamma *
                    (GetMaxSharedBufferSize().GetValue() - GetCurrentSharedBufferSize().GetValue());
                return QueueSize(QueueSizeUnit::PACKETS, m_p_threshold_h);
            }
            else if (alpha == alpha_l)
            {
                m_numConjestedQueuesLow = conjestedQueues;
                m_p_threshold_l =
                    alpha_l * (1 / m_numConjestedQueuesLow) * m_gamma *
                    (GetMaxSharedBufferSize().GetValue() - GetCurrentSharedBufferSize().GetValue());
                return QueueSize(QueueSizeUnit::PACKETS, m_p_threshold_l);
            }
        }
        if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::BYTES)
        {
            if (alpha == alpha_h)
            {
                m_numConjestedQueuesHigh = conjestedQueues;
                m_b_threshold_h = std::ceil(alpha_h * (1 / m_numConjestedQueuesHigh) * m_gamma *
                                            (GetMaxSharedBufferSize().GetValue() -
                                             GetCurrentSharedBufferSize().GetValue()));
                return QueueSize(QueueSizeUnit::PACKETS, m_b_threshold_h);
            }
            else if (alpha == alpha_l)
            {
                m_numConjestedQueuesLow = conjestedQueues;
                m_p_threshold_l = std::ceil(alpha_l * (1 / m_numConjestedQueuesLow) * m_gamma *
                                            (GetMaxSharedBufferSize().GetValue() -
                                             GetCurrentSharedBufferSize().GetValue()));
                return QueueSize(QueueSizeUnit::PACKETS, m_b_threshold_l);
            }
        }
        NS_ABORT_MSG("Unknown Threshod unit");
    }
}

QueueSize
TrafficControlLayer::GetNumOfTotalEnqueuedPriorityPacketsinQueues(uint32_t queue_priority)
{
    NS_LOG_FUNCTION(this);

    /// Create a vector to hold pointers to all the NetDevices aggrigated to the Node
    std::vector<Ptr<NetDevice>>
        m_netDevicesList; // a List containing all the net devices on a speciffic Node
    for (const auto& element : m_netDevices)
    {
        m_netDevicesList.push_back(element.first);
    }

    // calculate the total number of enqueued packets in the Shared Buffer (from the beginning of the simulation) as the sum of all packets ever enqueued in all the 
    // sub queues

    m_nTotalEnqueuedPacketsInSharedBuffer = 0;

    /// calculate the number of the packets/bytes in the Shared Buffe
    for (size_t i = 0; i < m_netDevicesList.size(); i++)
    {
        Ptr<NetDevice> ndev = m_netDevicesList[i];
        std::string netDeviceName = Names::FindName(ndev); // Get the name of the net-device
        if (netDeviceName.find("switchDeviceOut")!=std::string::npos)
        {
            std::map<Ptr<NetDevice>, NetDeviceInfo>::iterator ndi = m_netDevices.find(ndev);
            if (ndi == m_netDevices.end() || !ndi->second.m_rootQueueDisc) // if no root-queue-disc is installed on the net-device not relevat at the moment
            {
                   break;
            //     Ptr<PointToPointNetDevice> p2pndev = DynamicCast<PointToPointNetDevice>(ndev);
            //     Ptr<Queue<Packet>> queue = p2pndev->GetQueue();
            //     totalEnqueuedPacketsInQueueCounter = queue->Get;
            }
            else // if we use a queue-disc on NetDevice as port
            {
                if (ndi->second.m_rootQueueDisc->GetNQueueDiscClasses() > 1) // if we use multiple queues/port
                {
                    for (size_t j = 0; j < ndi->second.m_rootQueueDisc->GetNQueueDiscClasses(); j++)
                    {
                        Ptr<QueueDisc> qDisc = ndi->second.m_rootQueueDisc->GetQueueDiscClass(j)->GetQueueDisc();
                        if (queue_priority == 1) // 1 is high priority
                        {
                            totalEnqueuedPacketsInQueueCounter = qDisc->GetStats().nTotalEnqueuedPackets_h;
                            m_nTotalEnqueuedPacketsInSharedBuffer += totalEnqueuedPacketsInQueueCounter;
                        }
                        else if (queue_priority == 2) // 2 is low priority
                        {
                            totalEnqueuedPacketsInQueueCounter = qDisc->GetStats().nTotalEnqueuedPackets_l;
                            m_nTotalEnqueuedPacketsInSharedBuffer += totalEnqueuedPacketsInQueueCounter;
                        }  
                    }
                }
                else // if we use single queue/port
                {
                    Ptr<QueueDisc> qDisc = ndi->second.m_rootQueueDisc;
                    if (queue_priority == 1) // 1 is high priority
                    {
                        totalEnqueuedPacketsInQueueCounter = qDisc->GetStats().nTotalEnqueuedPackets_h;
                        m_nTotalEnqueuedPacketsInSharedBuffer += totalEnqueuedPacketsInQueueCounter;
                    }
                    else if (queue_priority == 2) // 2 is low priority
                    {
                        totalEnqueuedPacketsInQueueCounter = qDisc->GetStats().nTotalEnqueuedPackets_l;
                        m_nTotalEnqueuedPacketsInSharedBuffer += totalEnqueuedPacketsInQueueCounter;
                    } 
                }
            }
        }
    }
    if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::PACKETS)
    {
        return QueueSize(QueueSizeUnit::PACKETS, m_nTotalEnqueuedPacketsInSharedBuffer);
    }
    NS_ABORT_MSG("Unknown queue size unit");
}

QueueSize
TrafficControlLayer::GetNumOfTotalEnqueuedPacketsinQueues()
{
    NS_LOG_FUNCTION(this);

    /// Create a vector to hold pointers to all the NetDevices aggrigated to the Node
    std::vector<Ptr<NetDevice>>
        m_netDevicesList; // a List containing all the net devices on a speciffic Node
    for (const auto& element : m_netDevices)
    {
        m_netDevicesList.push_back(element.first);
    }

    // calculate the total number of enqueued packets in the Shared Buffer (from the beginning of the simulation) as the sum of all packets ever enqueued in all the 
    // sub queues

    m_nTotalEnqueuedPacketsInSharedBuffer = 0;

    /// calculate the number of the packets/bytes in the Shared Buffe
    for (size_t i = 0; i < m_netDevicesList.size(); i++)
    {
        Ptr<NetDevice> ndev = m_netDevicesList[i];
        std::string netDeviceName = Names::FindName(ndev); // Get the name of the net-device
        if (netDeviceName.find("switchDeviceOut")!=std::string::npos)
        {
            std::map<Ptr<NetDevice>, NetDeviceInfo>::iterator ndi = m_netDevices.find(ndev);
            if (ndi == m_netDevices.end() || !ndi->second.m_rootQueueDisc) // if no root-queue-disc is installed on the net-device not relevat at the moment
            {
                   break;
            //     Ptr<PointToPointNetDevice> p2pndev = DynamicCast<PointToPointNetDevice>(ndev);
            //     Ptr<Queue<Packet>> queue = p2pndev->GetQueue();
            //     totalEnqueuedPacketsInQueueCounter = queue->Get;
            }
            else // if we use a queue-disc on NetDevice as port
            {
                if (ndi->second.m_rootQueueDisc->GetNQueueDiscClasses() > 1) // if we use multiple queues/port
                {
                    for (size_t j = 0; j < ndi->second.m_rootQueueDisc->GetNQueueDiscClasses(); j++)
                    {
                        Ptr<QueueDisc> qDisc = ndi->second.m_rootQueueDisc->GetQueueDiscClass(j)->GetQueueDisc();
                        totalEnqueuedPacketsInQueueCounter = qDisc->GetStats().nTotalEnqueuedPackets;
                        m_nTotalEnqueuedPacketsInSharedBuffer += totalEnqueuedPacketsInQueueCounter;
                    }
                }
                else // if we use single queue/port
                {
                    Ptr<QueueDisc> qDisc = ndi->second.m_rootQueueDisc;
                    totalEnqueuedPacketsInQueueCounter = qDisc->GetStats().nTotalEnqueuedPackets;
                    m_nTotalEnqueuedPacketsInSharedBuffer += totalEnqueuedPacketsInQueueCounter;
                }
            }
        }
    }
    if (GetMaxSharedBufferSize().GetUnit() == QueueSizeUnit::PACKETS)
    {
        return QueueSize(QueueSizeUnit::PACKETS, m_nTotalEnqueuedPacketsInSharedBuffer);
    }
    NS_ABORT_MSG("Unknown queue size unit");
}

double_t 
TrafficControlLayer::RoundToOneDecimal(double_t num) 
{
    num = std::round(num * 10) / 10;
    return num;
}

double_t
TrafficControlLayer::EstimateNewLocalD()
{
    // we calculate the new d value (mouse/elephant probability) as 
    m_estimated_mice_elephant_prob = 0;

    DataSet dataset;
    dataset.LoadData("List_Predictive_Packets_Dropped_by_SharedBuffer.dat");

    DataPoint lastRow = dataset.GetLastRow();
    DataPoint currentRow = {0.0, 0.0, 0.0, 0.0}; // initilize currentRow to zeros before filling it with the actual data
    if ((lastRow.time - (1 + m_Tau)*1e+9) > 0) // make sure that Time (for Predictive model) that has passed is > Tau
    {
        double windowStartTime = lastRow.time - m_Tau*1e+9;
        currentRow = dataset.GetRow(windowStartTime);
        std::cout << "winow length [nSec]: " << lastRow.time - currentRow.time << std::endl;
    }
    else  // if Time (for Predictive model) that has passed is <= Tau, take the maximum window avalible
    {
        currentRow = dataset.GetFirstRow();
        std::cout << "winow length [nSec]: " << lastRow.time - currentRow.time << std::endl;
    }

    // for debug:
    std::cout << "Window start time: " << currentRow.time << std::endl;
    std::cout << "Total sum of packets: " << currentRow.var1 << std::endl;
    std::cout << "Sum of high priority packets: " << currentRow.var2 << std::endl;
    std::cout << "Sum of low priority packets: " << currentRow.var3 << std::endl;

    std::cout << "Window end time: " << lastRow.time << std::endl;
    std::cout << "Total sum of packets: " << lastRow.var1 << std::endl;
    std::cout << "Sum of high priority packets " << lastRow.var2 << std::endl;
    std::cout << "Sum of low priority packets: " << lastRow.var3 << std::endl;

    m_predictedArrivingTraffic = lastRow.var1 - currentRow.var1;
    m_predictedArrivingHighPriorityTraffic = lastRow.var2 - currentRow.var2;

    m_estimated_mice_elephant_prob = RoundToOneDecimal(m_predictedArrivingHighPriorityTraffic / m_predictedArrivingTraffic);

    std::cout << "the new predictive d value is: " << m_estimated_mice_elephant_prob << std::endl;
    
    // for debug, save a list of all the estimated d values to a file
    std::ofstream estimatedDValuesOutputFile("Estimated_D_Values.dat", std::ios::app); 
                estimatedDValuesOutputFile << m_estimated_mice_elephant_prob << std::endl;
                estimatedDValuesOutputFile.close();
    return m_estimated_mice_elephant_prob;
}

std::pair<double_t, double_t>
TrafficControlLayer::GetNewAlphaHighAndLow(Ptr<NetDevice> device, uint32_t mice_elephant_prob_val)
{
    double_t alpha_h, alpha_l;
    // assign alpha high/low values based on previously done optimization

    // inorder to diferentiate between scenarios with 2 queues/port and 5 queues/port:
    Ptr<NetDevice> ndev = device;
    std::map<Ptr<NetDevice>, NetDeviceInfo>::iterator ndi = m_netDevices.find(ndev);

    if (ndi->second.m_rootQueueDisc->GetNQueueDiscClasses() == 5) // always in use in the current model
    {
        if (m_usedAlgorythm.compare("PredictiveDT") == 0 || m_usedAlgorythm.compare("DT") == 0)
        {
            switch (mice_elephant_prob_val)
            {
            case 1:
                alpha_h = 11;
                alpha_l = 9;
                break;
            case 2:
                alpha_h = 11;
                alpha_l = 9;
                break;
            case 3:
                alpha_h = 13;
                alpha_l = 7;
                break;
            case 4:
                alpha_h = 17;
                alpha_l = 3;
                break;
            case 5:
                alpha_h = 18;
                alpha_l = 2;
                break;
            case 6:
                alpha_h = 19;
                alpha_l = 1;
                break;
            case 7:
                alpha_h = 20;
                alpha_l = 0.5;
                break;
            case 8:
                alpha_h = 20;
                alpha_l = 0.5;
                break;
            case 9:
                alpha_h = 20;
                alpha_l = 0.5;
                break;
            case 10:  // saturated
                alpha_h = 20;
                alpha_l = 0.5;
                break;
            default:
                break;
            }
        }
        else if (m_usedAlgorythm.compare("PredictiveFB") == 0 || m_usedAlgorythm.compare("FB") == 0)
        {
            switch (mice_elephant_prob_val)
            {
            case 1:
                alpha_h = 1;
                alpha_l = 19;
                break;
            case 2:
                alpha_h = 2;
                alpha_l = 18;
                break;
            case 3:
                alpha_h = 6;
                alpha_l = 14;
                break;
            case 4:
                alpha_h = 9;
                alpha_l = 11;
                break;
            case 5:
                alpha_h = 15;
                alpha_l = 5;
                break;
            case 6:
                alpha_h = 18;
                alpha_l = 2;
                break;
            case 7:
                alpha_h = 20;
                alpha_l = 0.5;
                break;
            case 8:
                alpha_h = 20;
                alpha_l = 0.5;
                break;
            case 9:
                alpha_h = 20;
                alpha_l = 0.5;
                break;
            case 10:  // saturated
                alpha_h = 20;
                alpha_l = 0.5;
                break;
            default:
                break;
            }
        }
        else if (m_usedAlgorythm.compare("PredictiveFB_v1") == 0) // currently not in use
        {
            switch (mice_elephant_prob_val)
            {
            case 1:
                alpha_h = 3;
                alpha_l = 17;
                break;
            case 2:
                alpha_h = 6;
                alpha_l = 14;
                break;
            case 3:
                alpha_h = 9;
                alpha_l = 11;
                break;
            case 4:
                alpha_h = 11;
                alpha_l = 9;
                break;
            case 5:
                alpha_h = 15;
                alpha_l = 5;
                break;
            case 6:
                alpha_h = 17;
                alpha_l = 3;
                break;
            case 7:
                alpha_h = 20;
                alpha_l = 0.5;
                break;
            case 8:
                alpha_h = 20;
                alpha_l = 0.5;
                break;
            case 9:
                alpha_h = 20;
                alpha_l = 0.5;
                break;
            default:
                break;
            }
        }
    }

    return std::make_pair(alpha_h, alpha_l);
}

void
TrafficControlLayer::DropBeforeEnqueue(Ptr<const QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    // save lost packets statistics in time t+Tau, in a sepparate file for Predictive Model
    std::string nodeName = Names::FindName(m_node); // Get the name of the Node
    // std::cout << "Node Name is: " << nodeName << std::endl;

    // delete the list of predicted traffic from previous runs
    if (m_stats.nTotalDroppedPackets == 0 && nodeName.compare("RouterPredict") == 0) // means that this is the first time the simulation get's here
    {
        std::remove("List_Predictive_Packets_Dropped_by_SharedBuffer.dat");
    }

    m_stats.nTotalDroppedPackets++;
    m_stats.nTotalDroppedBytes += item->GetSize();

    if (item->GetPacket()->PeekPacketTag(flowPrioTag))
    {
        m_flow_priority = flowPrioTag.GetSimpleValue();
    }

    if (m_flow_priority == 1)
    {
        m_stats.nTotalDroppedPacketsHighPriority++;
        m_stats.nTotalDroppedBytesHighPriority += item->GetSize();
    }
    else if (m_flow_priority == 2)
    {
        m_stats.nTotalDroppedPacketsLowPriority++;
        m_stats.nTotalDroppedBytesLowPriority += item->GetSize();
    }

    if (nodeName.compare("RouterPredict") == 0)
    {
        // create a list of time-stamped sums of dropped Predicted Packets
        std::ofstream listPredictedPacketsDroppedFile("List_Predictive_Packets_Dropped_by_SharedBuffer.dat",
                                 std::ios::app);
        listPredictedPacketsDroppedFile << Simulator::Now () << " "; 
        listPredictedPacketsDroppedFile << m_stats.nTotalDroppedPackets << " ";
        listPredictedPacketsDroppedFile << m_stats.nTotalDroppedPacketsHighPriority << " ";
        listPredictedPacketsDroppedFile << m_stats.nTotalDroppedPacketsLowPriority << std::endl;
        listPredictedPacketsDroppedFile.close();
    }

    NS_LOG_DEBUG("Total High Priority packets/bytes dropped by Traffic Controll Layer: "
                 << m_stats.nTotalDroppedPacketsHighPriority << " / "
                 << m_stats.nTotalDroppedBytesHighPriority);
    NS_LOG_DEBUG("Total Low Priority packets/bytes dropped by Traffic Controll Layer: "
                 << m_stats.nTotalDroppedPacketsLowPriority << " / "
                 << m_stats.nTotalDroppedBytesLowPriority);
    NS_LOG_DEBUG("Total packets/bytes dropped by Traffic Controll Layer: "
                 << m_stats.nTotalDroppedPackets << " / " << m_stats.nTotalDroppedBytes);
    NS_LOG_LOGIC("m_dropped (p)");

    m_dropped(item->GetPacket());
}

void
TrafficControlLayer::ScanDevices()
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT_MSG(m_node, "Cannot run ScanDevices without an aggregated node");

    NS_LOG_DEBUG("Scanning devices on node " << m_node->GetId());
    for (uint32_t i = 0; i < m_node->GetNDevices(); i++)
    {
        NS_LOG_DEBUG("Scanning devices on node " << m_node->GetId());
        Ptr<NetDevice> dev = m_node->GetDevice(i);
        NS_LOG_DEBUG("Checking device " << i << " with pointer " << dev << " of type "
                                        << dev->GetInstanceTypeId().GetName());

        // note: there may be no NetDeviceQueueInterface aggregated to the device
        Ptr<NetDeviceQueueInterface> ndqi = dev->GetObject<NetDeviceQueueInterface>();
        NS_LOG_DEBUG("Pointer to NetDeviceQueueInterface: " << ndqi);

        auto ndi = m_netDevices.find(dev);

        if (ndi != m_netDevices.end())
        {
            NS_LOG_DEBUG("Device entry found; installing NetDeviceQueueInterface pointer "
                         << ndqi << " to internal map");
            ndi->second.m_ndqi = ndqi;
        }
        else if (ndqi)
        // if no entry for the device is found, it means that no queue disc has been
        // installed. Nonetheless, create an entry for the device and store a pointer
        // to the NetDeviceQueueInterface object if the latter is not null, because
        // the Traffic Control layer checks whether the device queue is stopped even
        // when there is no queue disc.
        {
            NS_LOG_DEBUG("No device entry found; create entry for device and store pointer to "
                         "NetDeviceQueueInterface: "
                         << ndqi);
            m_netDevices[dev] = {nullptr, ndqi, QueueDiscVector()};
            ndi = m_netDevices.find(dev);
        }

        // if a queue disc is installed, set the wake callbacks on netdevice queues
        if (ndi != m_netDevices.end() && ndi->second.m_rootQueueDisc)
        {
            NS_LOG_DEBUG("Setting the wake callbacks on NetDevice queues");
            ndi->second.m_queueDiscsToWake.clear();

            if (ndqi)
            {
                for (std::size_t i = 0; i < ndqi->GetNTxQueues(); i++)
                {
                    Ptr<QueueDisc> qd;

                    if (ndi->second.m_rootQueueDisc->GetWakeMode() == QueueDisc::WAKE_ROOT)
                    {
                        qd = ndi->second.m_rootQueueDisc;
                    }
                    else if (ndi->second.m_rootQueueDisc->GetWakeMode() == QueueDisc::WAKE_CHILD)
                    {
                        NS_ABORT_MSG_IF(ndi->second.m_rootQueueDisc->GetNQueueDiscClasses() !=
                                            ndqi->GetNTxQueues(),
                                        "The number of child queue discs does not match the number "
                                        "of netdevice queues");

                        qd = ndi->second.m_rootQueueDisc->GetQueueDiscClass(i)->GetQueueDisc();
                    }
                    else
                    {
                        NS_ABORT_MSG("Invalid wake mode");
                    }

                    ndqi->GetTxQueue(i)->SetWakeCallback(MakeCallback(&QueueDisc::Run, qd));
                    ndi->second.m_queueDiscsToWake.push_back(qd);
                }
            }
            else
            {
                ndi->second.m_queueDiscsToWake.push_back(ndi->second.m_rootQueueDisc);
            }

            // set the NetDeviceQueueInterface object and the SendCallback on the queue discs
            // into which packets are enqueued and dequeued by calling Run
            for (auto& q : ndi->second.m_queueDiscsToWake)
            {
                q->SetNetDeviceQueueInterface(ndqi);
                q->SetSendCallback([dev](Ptr<QueueDiscItem> item) {
                    dev->Send(item->GetPacket(), item->GetAddress(), item->GetProtocol());
                });
            }
        }
    }
}

void
TrafficControlLayer::SetRootQueueDiscOnDevice(Ptr<NetDevice> device, Ptr<QueueDisc> qDisc)
{
    NS_LOG_FUNCTION(this << device << qDisc);

    auto ndi = m_netDevices.find(device);

    if (ndi == m_netDevices.end())
    {
        // No entry found for this device. Create one.
        m_netDevices[device] = {qDisc, nullptr, QueueDiscVector()};
    }
    else
    {
        NS_ABORT_MSG_IF(ndi->second.m_rootQueueDisc,
                        "Cannot install a root queue disc on a device already having one. "
                        "Delete the existing queue disc first.");

        ndi->second.m_rootQueueDisc = qDisc;
    }
}

Ptr<QueueDisc>
TrafficControlLayer::GetRootQueueDiscOnDevice(Ptr<NetDevice> device) const
{
    NS_LOG_FUNCTION(this << device);

    auto ndi = m_netDevices.find(device);

    if (ndi == m_netDevices.end())
    {
        return nullptr;
    }
    return ndi->second.m_rootQueueDisc;
}

Ptr<QueueDisc>
TrafficControlLayer::GetRootQueueDiscOnDeviceByIndex(uint32_t index) const
{
    NS_LOG_FUNCTION(this << index);
    return GetRootQueueDiscOnDevice(m_node->GetDevice(index));
}

void
TrafficControlLayer::DeleteRootQueueDiscOnDevice(Ptr<NetDevice> device)
{
    NS_LOG_FUNCTION(this << device);

    auto ndi = m_netDevices.find(device);

    NS_ASSERT_MSG(ndi != m_netDevices.end() && ndi->second.m_rootQueueDisc,
                  "No root queue disc installed on device " << device);

    // remove the root queue disc
    ndi->second.m_rootQueueDisc = nullptr;
    for (auto& q : ndi->second.m_queueDiscsToWake)
    {
        q->SetNetDeviceQueueInterface(nullptr);
        q->SetSendCallback(nullptr);
    }
    ndi->second.m_queueDiscsToWake.clear();

    Ptr<NetDeviceQueueInterface> ndqi = ndi->second.m_ndqi;
    if (ndqi)
    {
        // remove configured callbacks, if any
        for (std::size_t i = 0; i < ndqi->GetNTxQueues(); i++)
        {
            ndqi->GetTxQueue(i)->SetWakeCallback(MakeNullCallback<void>());
        }
    }
    else
    {
        // remove the empty entry
        m_netDevices.erase(ndi);
    }
}

void
TrafficControlLayer::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    m_node = node;
}

void
TrafficControlLayer::NotifyNewAggregate()
{
    NS_LOG_FUNCTION(this);
    if (!m_node)
    {
        Ptr<Node> node = this->GetObject<Node>();
        // verify that it's a valid node and that
        // the node was not set before
        if (node)
        {
            this->SetNode(node);
        }
    }
    Object::NotifyNewAggregate();
}

uint32_t
TrafficControlLayer::GetNDevices() const
{
    return m_node->GetNDevices();
}

void
TrafficControlLayer::Receive(Ptr<NetDevice> device,
                             Ptr<const Packet> p,
                             uint16_t protocol,
                             const Address& from,
                             const Address& to,
                             NetDevice::PacketType packetType)
{
    NS_LOG_FUNCTION(this << device << p << protocol << from << to << packetType);

    bool found = false;

    for (auto i = m_handlers.begin(); i != m_handlers.end(); i++)
    {
        if (!i->device || (i->device == device))
        {
            if (i->protocol == 0 || i->protocol == protocol)
            {
                NS_LOG_DEBUG("Found handler for packet " << p << ", protocol " << protocol
                                                         << " and NetDevice " << device
                                                         << ". Send packet up");
                i->handler(device, p, protocol, from, to, packetType);
                found = true;
            }
        }
    }

    NS_ABORT_MSG_IF(!found,
                    "Handler for protocol " << p << " and device " << device
                                            << " not found. It isn't forwarded up; it dies here.");
}

void
TrafficControlLayer::Send(Ptr<NetDevice> device, Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << device << item);

    NS_LOG_DEBUG("Send packet to device " << device << " protocol number " << item->GetProtocol());

    Ptr<NetDeviceQueueInterface> devQueueIface;
    auto ndi = m_netDevices.find(device);

    if (ndi != m_netDevices.end())
    {
        devQueueIface = ndi->second.m_ndqi;
    }

    // determine the transmission queue of the device where the packet will be enqueued
    std::size_t txq = 0;
    if (devQueueIface && devQueueIface->GetNTxQueues() > 1)
    {
        txq = devQueueIface->GetSelectQueueCallback()(item);
        // otherwise, Linux determines the queue index by using a hash function
        // and associates such index to the socket which the packet belongs to,
        // so that subsequent packets of the same socket will be mapped to the
        // same tx queue (__netdev_pick_tx function in net/core/dev.c). It is
        // pointless to implement this in ns-3 because currently the multi-queue
        // devices provide a select queue callback
    }

    NS_ASSERT(!devQueueIface || txq < devQueueIface->GetNTxQueues());

    if (ndi == m_netDevices.end() ||
        !ndi->second.m_rootQueueDisc) // if there's no queue-disc installed on the NetDevice
    {
        // The device has no attached queue disc, thus add the header to the packet and
        // send it directly to the device if the selected queue is not stopped
        item->AddHeader();
        if (!devQueueIface || !devQueueIface->GetTxQueue(txq)->IsStopped())
        {
            // a single queue device makes no use of the priority tag
            if (!devQueueIface || devQueueIface->GetNTxQueues() == 1)
            {
                SocketPriorityTag priorityTag;
                item->GetPacket()->RemovePacketTag(priorityTag);
            }

            if (m_useSharedBuffer) // if user choses to activate Shared-Buffer algorythem
            {
                // for Shared Buffer. in case no queue-disc is installed on the NetDevice////////
                // std::cout << "SharedBuffer, No QueueDisc" << std::endl;

                std::string nodeName = Names::FindName(m_node); // Get the name of the Node
                // std::cout << "Node Name is: " << nodeName << std::endl;
                if (nodeName.compare("Router") == 0)
                {
                    // for debug:
                    std::cout << "Node Name is: " << nodeName << std::endl;
                    // std::cout << "Packet: " << item->GetPacket() << " is sent to Port on
                    // NetDevice: " << device << std::endl;
                    std::cout << "Current Shared Buffer size is "
                              << GetCurrentSharedBufferSize().GetValue()
                              << " out of: " << GetMaxSharedBufferSize().GetValue() << std::endl;

                    // for tracing
                    m_traceSharedBufferPackets = GetCurrentSharedBufferSize().GetValue();
                    m_nPackets_trace_High_InSharedQueue =
                        GetNumOfHighPriorityPacketsInSharedQueue().GetValue();
                    m_nPackets_trace_Low_InSharedQueue =
                        GetNumOfLowPriorityPacketsInSharedQueue().GetValue();

                    Ptr<PointToPointNetDevice> p2pndev = DynamicCast<PointToPointNetDevice>(device);
                    Ptr<Queue<Packet>> queue = p2pndev->GetQueue();

                    // determine the port index of current net-device:
                    std::string netDeviceName = Names::FindName(device); // Get the name of the net-device
                    size_t txPortIndex = 0;
                    if (netDeviceName.find("switchDeviceOut")!=std::string::npos)
                    {
                        size_t netDeviceIndex = GetNetDeviceIndex(device);
                        // std::cout << "Index of '" << device << "': " << netDeviceIndex << std::endl;
                        // txPortIndex = netDeviceIndex % 2; // if the current net-device is a "switchDeviceOut" then,
                        // the port index is the netDeviceIndex modulu 2.
                        
                        txPortIndex = netDeviceName.back() - '0';
                        // std::cout << "Index of '" << device << "': " << txPortIndex << std::endl;
                    }

                    // set a besic Packet clasification based on arbitrary Tag from recieved packet:
                    // flow_priority = 1 is high priority, flow_priority = 2 is low priority
                    if (item->GetPacket()->PeekPacketTag(flowPrioTag))
                    {
                        m_flow_priority = flowPrioTag.GetSimpleValue();
                    }

                    // for debug:
                    // std::cout << "d value assigned by OnOff Application is: "
                    //           << item->GetPacket()->PeekPacketTag(miceElephantProbTag) << std::endl;

                    // perform enqueueing process based on incoming flow priority
                    if (m_flow_priority == 1)
                    {
                        m_alpha = m_alpha_h;
                        if (m_usedAlgorythm.compare("DT") == 0)
                        {
                            if (queue->GetNumOfHighPrioPacketsInQueue().GetValue() <
                                GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h).GetValue())
                            {
                                // to trace Threshold per port
                                if (txPortIndex == 0)
                                {
                                    m_p_trace_threshold_h_0 =
                                        GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h)
                                            .GetValue(); // for tracing
                                }
                                else if (txPortIndex == 1)
                                {
                                    m_p_trace_threshold_h_1 =
                                        GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h)
                                            .GetValue(); // for tracing
                                }
                                device->Send(item->GetPacket(),
                                             item->GetAddress(),
                                             item->GetProtocol());
                            }
                            else
                            {
                                std::cout << "High Priority packet was dropped by Shared-Buffer"
                                          << std::endl;
                                std::cout << "High Priority Threshold is:"
                                          << GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h)
                                                 .GetValue()
                                          << std::endl;
                                std::cout
                                    << "Number of High Priority packets in queue on net-device: "
                                    << device << " is: " << queue->GetNumOfHighPrioPacketsInQueue()
                                    << std::endl;
                                DropBeforeEnqueue(item);
                            }
                        }
                        else if (m_usedAlgorythm.compare("FB") == 0)
                        {
                            // step 1: calculate the Normalized dequeue BW of the designated queue:
                            float gamma_i = GetNormalizedDequeueBandWidth_v1(device, 0);
                            
                            // step 2: get the total number of conjested queues in shared buffer
                            int conjestedQueues = 0; // initilize to 0
                            conjestedQueues =
                                GetNumOfPriorityConjestedQueuesInSharedQueue(m_flow_priority);
                            
                            // for debug:
                            std::cout << "Num of congested queues of priority: " << int(m_flow_priority)
                                                            << " is: " << conjestedQueues << std::endl;
                            // std::cout << "Num of total congested queues " << conjestedQueues << std::endl;
                             
                            // step 3: use calculated gamma_i(t) and Np(t) to calculate
                            // the FB_Threshold_c(t)
                            if (queue->GetNumOfHighPrioPacketsInQueue().GetValue() <
                                GetQueueThreshold_FB(m_alpha,
                                                        m_alpha_l,
                                                        m_alpha_h,
                                                        conjestedQueues,
                                                        gamma_i)
                                    .GetValue())
                            {
                                // to trace Threshold per port
                                if (txPortIndex == 0)
                                {
                                    m_p_trace_threshold_h_0 =
                                        GetQueueThreshold_FB(m_alpha,
                                                                m_alpha_l,
                                                                m_alpha_h,
                                                                conjestedQueues,
                                                                gamma_i)
                                            .GetValue(); // for tracing
                                }
                                else if (txPortIndex == 1)
                                {
                                    m_p_trace_threshold_h_1 =
                                        GetQueueThreshold_FB(m_alpha,
                                                                m_alpha_l,
                                                                m_alpha_h,
                                                                conjestedQueues,
                                                                gamma_i)
                                            .GetValue(); // for tracing
                                }
                                device->Send(item->GetPacket(),
                                             item->GetAddress(),
                                             item->GetProtocol());
                            }
                            else
                            {
                                std::cout << "High Priority packet was dropped by Shared-Buffer"
                                          << std::endl;
                                std::cout << "High Priority Threshold is:"
                                          << GetQueueThreshold_FB(m_alpha,
                                                                     m_alpha_l,
                                                                     m_alpha_h,
                                                                     conjestedQueues,
                                                                     gamma_i)
                                                 .GetValue()
                                          << std::endl;
                                std::cout
                                    << "Number of High Priority packets in queue on net-device: "
                                    << device << " is: " << queue->GetNumOfHighPrioPacketsInQueue()
                                    << std::endl;
                                std::cout << "Number of High Priority Conjested queues: "
                                          << conjestedQueues << std::endl;
                                DropBeforeEnqueue(item);
                            }
                        }
                        else
                        {
                            NS_ABORT_MSG("unrecognised traffic management algorythm "
                                         << m_usedAlgorythm);
                        }
                    }
                    else
                    {
                        m_alpha = m_alpha_l;
                        if (m_usedAlgorythm.compare("DT") == 0)
                        {
                            if (queue->GetNumOfLowPrioPacketsInQueue().GetValue() <
                                GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h).GetValue())
                            {
                                // to trace Threshold per port
                                if (txPortIndex == 0)
                                {
                                    m_p_trace_threshold_l_0 =
                                        GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h)
                                            .GetValue(); // for tracing
                                }
                                else if (txPortIndex == 1)
                                {
                                    m_p_trace_threshold_l_1 =
                                        GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h)
                                            .GetValue(); // for tracing
                                }
                                device->Send(item->GetPacket(),
                                             item->GetAddress(),
                                             item->GetProtocol());
                            }
                            else
                            {
                                std::cout << "Low Priority packet was dropped by Shared-Buffer"
                                          << std::endl;
                                std::cout << "Low Priority Threshold is:"
                                          << GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h)
                                                 .GetValue()
                                          << std::endl;
                                std::cout
                                    << "Number of Low Priority packets in queue on net-device: "
                                    << device << " is: " << queue->GetNumOfLowPrioPacketsInQueue()
                                    << std::endl;
                                DropBeforeEnqueue(item);
                            }
                        }
                        else if (m_usedAlgorythm.compare("FB") == 0)
                        {
                            // step 1: calculate the Normalized dequeue BW of the designated queue:
                            float gamma_i = GetNormalizedDequeueBandWidth_v1(device, 0);
                            // step 2: get the total number of conjested queues in shared buffer
                            int conjestedQueues = 0;
                            conjestedQueues = 
                                GetNumOfPriorityConjestedQueuesInSharedQueue(m_flow_priority);

                            if (queue->GetNumOfLowPrioPacketsInQueue().GetValue() <
                                GetQueueThreshold_FB(m_alpha,
                                                        m_alpha_l,
                                                        m_alpha_h,
                                                        conjestedQueues,
                                                        gamma_i)
                                    .GetValue())
                            {
                                // to trace Threshold per port
                                if (txPortIndex == 0)
                                {
                                    m_p_trace_threshold_l_0 =
                                        GetQueueThreshold_FB(m_alpha,
                                                                m_alpha_l,
                                                                m_alpha_h,
                                                                conjestedQueues,
                                                                gamma_i)
                                            .GetValue(); // for tracing
                                }
                                else if (txPortIndex == 1)
                                {
                                    m_p_trace_threshold_l_1 =
                                        GetQueueThreshold_FB(m_alpha,
                                                                m_alpha_l,
                                                                m_alpha_h,
                                                                conjestedQueues,
                                                                gamma_i)
                                            .GetValue(); // for tracing
                                }
                                device->Send(item->GetPacket(),
                                             item->GetAddress(),
                                             item->GetProtocol());
                            }
                            else
                            {
                                std::cout << "Low Priority packet was dropped by Shared-Buffer"
                                          << std::endl;
                                std::cout << "Low Priority Threshold is:"
                                          << GetQueueThreshold_FB(m_alpha,
                                                                     m_alpha_l,
                                                                     m_alpha_h,
                                                                     conjestedQueues,
                                                                     gamma_i)
                                                 .GetValue()
                                          << std::endl;
                                std::cout
                                    << "Number of Low Priority packets in queue on net-device: "
                                    << device << " is: " << queue->GetNumOfLowPrioPacketsInQueue()
                                    << std::endl;
                                std::cout << "Number of Low Priority Conjested queues: "
                                          << conjestedQueues << std::endl;
                                DropBeforeEnqueue(item);
                            }
                        }
                        else
                        {
                            NS_ABORT_MSG("unrecognised traffic management algorythm "
                                         << m_usedAlgorythm);
                        }
                    }
                }
                //////////////////////////////////////////////////////////////////////////////////
                else // if not router
                {
                    device->Send(item->GetPacket(), item->GetAddress(), item->GetProtocol());
                }
            }
            else // if Shared Buffer was not selected
            {
                device->Send(item->GetPacket(), item->GetAddress(), item->GetProtocol());
            }
        }
        else
        {
            m_dropped(item->GetPacket());
        }
    }
    else // if there's a queue-disc installed on the net-device
    {
        if (m_useSharedBuffer)
        {
            std::string nodeName = Names::FindName(m_node); // Get the name of the Node
            std::cout << "Node Name is: " << nodeName << std::endl;
            if (nodeName.compare("RouterPredict") == 0)
            {
                // for OnOff times DEBUG, log each packet arrival time in a sepparate file:
                // std::ofstream packetArrivalTimesOutputFile("Predictive_Packet_Arrival_Times.dat", std::ios::app); 
                // packetArrivalTimesOutputFile << Simulator::Now () << std::endl;
                // packetArrivalTimesOutputFile.close();

                DropBeforeEnqueue(item); // all Predictive packets are logged in the DropBeforeEnque function
            }
            else if (nodeName.find("Router") == 0)
            {
                // for OnOff times DEBUG, log each packet arrival time in a sepparate file:
                // std::ofstream packetArrivalTimesOutputFile("Packet_Arrival_Times.dat",
                //                             std::ios::app); // Replace with your desired file path
                // packetArrivalTimesOutputFile << Simulator::Now () - Seconds(0.2) << std::endl;  // normalize arriving packet times to be at t-Tau
                // packetArrivalTimesOutputFile.close();

                std::cout << "Current Shared Buffer size is "
                          << GetCurrentSharedBufferSize().GetValue()
                          << " out of: " << GetMaxSharedBufferSize().GetValue() << std::endl;
                
                // for tracing
                m_traceSharedBufferPackets = GetCurrentSharedBufferSize().GetValue();
                m_nPackets_trace_High_InSharedQueue =
                    GetNumOfHighPriorityPacketsInSharedQueue().GetValue();
                m_nPackets_trace_Low_InSharedQueue =
                    GetNumOfLowPriorityPacketsInSharedQueue().GetValue();

                // determine the port index of current net-device:
                std::string netDeviceName = Names::FindName(device); // Get the name of the net-device
                size_t txPortIndex = 0;
                if (netDeviceName.find("switchDeviceOut")!=std::string::npos)
                {
                    size_t netDeviceIndex = GetNetDeviceIndex(device);
                    // std::cout << "Index of '" << device << "': " << netDeviceIndex << std::endl;
                    // txPortIndex = netDeviceIndex % 2; // if the current net-device is a "switchDeviceOut" then the port index is the netDeviceIndex modulu 2
                    
                    txPortIndex = netDeviceName.back() - '0';
                    // std::cout << "Index of '" << device << "': " << txPortIndex << std::endl;
                }
                
                // Enqueue the packet in the queue disc associated with the netdevice queue
                // selected for the packet and try to dequeue packets from such queue disc
                item->SetTxQueueIndex(txq);
                Ptr<QueueDisc> qDisc = ndi->second.m_queueDiscsToWake[txq];
                NS_ASSERT(qDisc);
                Ptr<QueueDisc> internal_qDisc; // for the multiqueue case
                SocketIpTosTag ipTosTag;
                uint16_t subQueueIndex = m_tosPrioMap[0];
                if (ndi->second.m_rootQueueDisc->GetNQueueDiscClasses() > 1) // if we use multiple queues/port.
                {
                    // uint16_t subQueueIndex = m_tosPrioMap[0];
                    // SocketIpTosTag ipTosTag;
                    if (item->GetPacket()->PeekPacketTag(ipTosTag))
                    {
                        subQueueIndex = m_tosPrioMap[(ipTosTag.GetTos() / 2) & 0x0f];
                        //for debug:
                        std::cout << "packet: " << item->GetPacket() << " ToS : " << int(ipTosTag.GetTos()) << std::endl;
                    }
                    else
                    {
                        std::cout << "packet: " << item->GetPacket() << " ToS : " << 0 << std::endl;
                    }
                    ////////////////////////////////////////
                    internal_qDisc = qDisc->GetQueueDiscClass(subQueueIndex)->GetQueueDisc();
                    NS_ASSERT(internal_qDisc);
                    m_multiQueuePerPort = true;
                }
                else
                {
                    subQueueIndex = 0; // efectively it doesn't matter
                    // for debug:
                    if (item->GetPacket()->PeekPacketTag(ipTosTag))
                    {
                        std::cout << "packet: " << item->GetPacket() << " ToS : " << int(ipTosTag.GetTos()) << std::endl;
                    }
                    else
                    {
                        std::cout << "packet: " << item->GetPacket() << " ToS : " << 0 << std::endl;
                    }
                    ////////////////////////////
                    internal_qDisc = qDisc;
                }
                // read the miceElephantProb (D) Tag assigned by the user at the OnOff App
                // if D > 0 and adjustableAlphas = True than update Alpha High/Low values according
                // to D
                if (item->GetPacket()->PeekPacketTag(miceElephantProbTag) && m_adjustableAlphas)
                {
                    // std::cout << "d value assigned by OnOff Application is: " <<
                    // miceElephantProbTag.GetSimpleValue() << std::endl;
                    m_miceElephantProbValFromTag = miceElephantProbTag.GetSimpleValue();  // the retrived d value will be an Int (10*d)
                    alphas = GetNewAlphaHighAndLow(device, m_miceElephantProbValFromTag); //because the switch function in GetNewAlphaHighAndLow() operates with Ints
                    m_alpha_h = alphas.first;
                    m_alpha_l = alphas.second;
                }
                else if (m_usedAlgorythm.compare("PredictiveFB") == 0 || m_usedAlgorythm.compare("PredictiveDT") == 0)
                {
                    // //print the High/Low Priority and the total number of packets that pass during the time interval t-Tau/2: t+Tau/2
                    // std::cout << "the number of High Priority packets during the time interval"
                    //                 " t-Tau/2: t+Tau/2 is: "
                    //             << GetNumOfPassedPriorityPackets(1) + GetNumOfArrivingPriorityPackets(1) << std::endl;
                    // std::cout << "the number of Low Priority packets during the time interval"
                    //                 " t-Tau/2: t+Tau/2 is: "
                    //             << GetNumOfPassedPriorityPackets(2) + GetNumOfArrivingPriorityPackets(2) << std::endl;
                    // std::cout << "the total number of packets during the time interval"
                    //                 " t-Tau/2: t+Tau/2 is: "
                    //             << GetNumOfPassedPackets() + GetNumOfArrivingPackets() << std::endl;
                    predictedMiceElephantProbVal = EstimateNewLocalD();
                    alphas = GetNewAlphaHighAndLow(device, 10 * predictedMiceElephantProbVal); // the switch function in GetNewAlphaHighAndLow() operates with Ints
                    m_alpha_h = alphas.first;
                    m_alpha_l = alphas.second;
                }
                // set a besic Packet clasification based on arbitrary Tag from recieved packet:
                // flow_priority = 1 is high priority, flow_priority = 2 is low priority
                if (item->GetPacket()->PeekPacketTag(flowPrioTag))
                {
                    m_flow_priority = flowPrioTag.GetSimpleValue();
                }

                // perform enqueueing process based on incoming flow priority
                if (m_flow_priority == 1)
                {
                    m_alpha = m_alpha_h;
                    if (m_usedAlgorythm.compare("DT") == 0 || m_usedAlgorythm.compare("PredictiveDT") == 0)
                    {
                        if (internal_qDisc->GetNumOfHighPrioPacketsInQueue().GetValue() <
                            GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h).GetValue())
                        {
                            // to trace Threshold per port
                            if (nodeName.compare("Router") == 0)
                            {
                                if (txPortIndex == 0)
                                {
                                    m_p_trace_threshold_h_0 =
                                        GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h)
                                            .GetValue(); // for tracing
                                }
                                else if (txPortIndex == 1)
                                {
                                    m_p_trace_threshold_h_1 =
                                        GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h)
                                            .GetValue(); // for tracing
                                }
                            }
                            qDisc->Enqueue(item);
                            qDisc->Run();
                        }
                        else
                        {
                            std::cout << "High Priority packet was dropped by Shared-Buffer"
                                      << std::endl;
                            DropBeforeEnqueue(item);
                        }
                    }
                    else if (m_usedAlgorythm.compare("FB") == 0 || m_usedAlgorythm.compare("PredictiveFB") == 0)
                    {
                        // step 1: calculate the Normalized dequeue BW of the designated queue:
                        // float gamma_i = GetNormalizedDequeueBandWidth_v1(device, subQueueIndex);
                        float gamma_i = GetNormalizedDequeueBandWidth_v2(device, m_flow_priority, txPortIndex, subQueueIndex);
                        // for debug:
                        std::cout << "the normalized dequeue rate on port: " << device
                                  << " is: " << gamma_i << std::endl;
                        // step 2: get the total number of conjested queues in shared buffer
                        int conjestedQueues = 0; // initilize to 0
                        conjestedQueues =
                            GetNumOfPriorityConjestedQueuesInSharedQueue(m_flow_priority);
                        // for debug:
                        std::cout << "Num of congested queues of priority: " << int(m_flow_priority)
                                  << " is: " << conjestedQueues << std::endl;
                        // step 3: use calculated gamma_i(t) and Np(t) to calculate the
                        // FB_Threshold_c(t)
                        if (internal_qDisc->GetNumOfHighPrioPacketsInQueue().GetValue() <
                            GetQueueThreshold_FB(m_alpha,
                                                    m_alpha_l,
                                                    m_alpha_h,
                                                    conjestedQueues,
                                                    gamma_i)
                                .GetValue())
                        {
                            // to trace Threshold per port
                            if (nodeName.compare("Router") == 0)
                            {
                                if (txPortIndex == 0)
                                {
                                    m_p_trace_threshold_h_0 =
                                        GetQueueThreshold_FB(m_alpha,
                                                                m_alpha_l,
                                                                m_alpha_h,
                                                                conjestedQueues,
                                                                gamma_i)
                                            .GetValue(); // for tracing
                                }
                                else if (txPortIndex == 1)
                                {
                                    m_p_trace_threshold_h_1 =
                                        GetQueueThreshold_FB(m_alpha,
                                                                m_alpha_l,
                                                                m_alpha_h,
                                                                conjestedQueues,
                                                                gamma_i)
                                            .GetValue(); // for tracing
                                }
                            }
                            qDisc->Enqueue(item);
                            qDisc->Run();
                        }
                        else
                        {
                            std::cout << "High Priority packet was dropped by Shared-Buffer"
                                      << std::endl;
                            DropBeforeEnqueue(item);
                        }
                    }
                    else
                    {
                        NS_ABORT_MSG("unrecognised traffic management algorythm "
                                     << m_usedAlgorythm);
                    }
                }
                else
                {
                    m_alpha = m_alpha_l;
                    if (m_usedAlgorythm.compare("DT") == 0 || m_usedAlgorythm.compare("PredictiveDT") == 0)
                    {
                        if (internal_qDisc->GetNumOfLowPrioPacketsInQueue().GetValue() <
                            GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h).GetValue())
                        {
                            // to trace Threshold per port
                            if (nodeName.compare("Router") == 0)
                            {
                                if (txPortIndex == 0)
                                {
                                    m_p_trace_threshold_l_0 =
                                        GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h)
                                            .GetValue(); // for tracing
                                }
                                else if (txPortIndex == 1)
                                {
                                    m_p_trace_threshold_l_1 =
                                        GetQueueThreshold_DT(m_alpha, m_alpha_l, m_alpha_h)
                                            .GetValue(); // for tracing
                                }
                            }
                            qDisc->Enqueue(item);
                            qDisc->Run();
                        }
                        else
                        {
                            std::cout << "Low Priority packet was dropped by Shared-Buffer"
                                      << std::endl;
                            DropBeforeEnqueue(item);
                        }
                    }
                    else if (m_usedAlgorythm.compare("FB") == 0 || m_usedAlgorythm.compare("PredictiveFB") == 0)
                    {
                        // step 1: calculate the Normalized dequeue BW of the designated queue:
                        // float gamma_i = GetNormalizedDequeueBandWidth_v1(device, subQueueIndex);
                        float gamma_i = GetNormalizedDequeueBandWidth_v2(device, m_flow_priority, txPortIndex, subQueueIndex);
                        // for debug:
                        std::cout << "the normalized dequeue rate on port: " << device
                                  << " is: " << gamma_i << std::endl;
                        // step 2: get the total number of conjested queues in shared buffer
                        int conjestedQueues = 0; // initilize to 0
                        conjestedQueues =
                            GetNumOfPriorityConjestedQueuesInSharedQueue(m_flow_priority);
                        // for debug:
                        std::cout << "Num of congested queues of priority " << int(m_flow_priority)
                                  << " is: " << conjestedQueues << std::endl;
                        // step 3: use calculated gamma_i(t) and Np(t) to calculate the
                        // FB_Threshold_c(t)
                        if (internal_qDisc->GetNumOfLowPrioPacketsInQueue().GetValue() <
                            GetQueueThreshold_FB(m_alpha,
                                                    m_alpha_l,
                                                    m_alpha_h,
                                                    conjestedQueues,
                                                    gamma_i)
                                .GetValue())
                        {
                            // for debug loop issue:
                            std::cout << "number of Low Priority packets in queue: "
                                      << internal_qDisc->GetNumOfLowPrioPacketsInQueue().GetValue()
                                      << std::endl;
                            std::cout << "Low Priority packet threshold is: "
                                      << GetQueueThreshold_FB(m_alpha,
                                                                 m_alpha_l,
                                                                 m_alpha_h,
                                                                 conjestedQueues,
                                                                 gamma_i)
                                             .GetValue()
                                      << std::endl;

                            // to trace Threshold per port
                            if (nodeName.compare("Router") == 0)
                            {
                                if (txPortIndex == 0)
                                {
                                    m_p_trace_threshold_l_0 =
                                        GetQueueThreshold_FB(m_alpha,
                                                                m_alpha_l,
                                                                m_alpha_h,
                                                                conjestedQueues,
                                                                gamma_i)
                                            .GetValue(); // for tracing
                                }
                                else if (txPortIndex == 1)
                                {
                                    m_p_trace_threshold_l_1 =
                                        GetQueueThreshold_FB(m_alpha,
                                                                m_alpha_l,
                                                                m_alpha_h,
                                                                conjestedQueues,
                                                                gamma_i)
                                            .GetValue(); // for tracing
                                }
                            }
                            qDisc->Enqueue(item);
                            qDisc->Run();
                        }
                        else
                        {
                            std::cout << "Low Priority packet was dropped by Shared-Buffer"
                                      << std::endl;
                            DropBeforeEnqueue(item);
                        }
                    }
                    else
                    {
                        NS_ABORT_MSG("unrecognised traffic management algorythm "
                                     << m_usedAlgorythm);
                    }
                }
            }
            else // for a node that's not the router
            {
                // Enqueue the packet in the queue disc associated with the netdevice queue
                // selected for the packet and try to dequeue packets from such queue disc
                item->SetTxQueueIndex(txq);

                Ptr<QueueDisc> qDisc = ndi->second.m_queueDiscsToWake[txq];
                NS_ASSERT(qDisc);
                qDisc->Enqueue(item);
                qDisc->Run();
            }
        }
        else // if shared buffer is not used
        {
            // Enqueue the packet in the queue disc associated with the netdevice queue
            // selected for the packet and try to dequeue packets from such queue disc
            item->SetTxQueueIndex(txq);

            Ptr<QueueDisc> qDisc = ndi->second.m_queueDiscsToWake[txq];
            NS_ASSERT(qDisc);
            qDisc->Enqueue(item);
            qDisc->Run();
        }
    }
}

} // namespace ns3
