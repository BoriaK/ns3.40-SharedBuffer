/*
 * Copyright (c) 2017 Universita' degli Studi di Napoli Federico II
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
 * Authors:  Stefano Avallone <stavallo@unina.it>
 */

#include "round-robin-tos-queue-disc.h"

#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/pointer.h"
#include "ns3/socket.h"
// for modifyed dequeue:
#include "ns3/simulator.h"
#include "ns3/error-model.h"

#include <algorithm>
#include <iterator>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RoundRobinTosQueueDisc");

NS_OBJECT_ENSURE_REGISTERED(RoundRobinTosQueueDisc);

ATTRIBUTE_HELPER_CPP(TosMap);

std::ostream&
operator<<(std::ostream& os, const TosMap& tosMap)
{
    std::copy(tosMap.begin(), tosMap.end() - 1, std::ostream_iterator<uint16_t>(os, " "));
    os << tosMap.back();
    return os;
}

std::istream&
operator>>(std::istream& is, TosMap& tosMap)
{
    for (int i = 0; i < 16; i++)
    {
        if (!(is >> tosMap[i]))
        {
            NS_FATAL_ERROR("Incomplete TosMap specification ("
                           << i << " values provided, 16 required)");
        }
    }
    return is;
}

TypeId
RoundRobinTosQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::RoundRobinTosQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<RoundRobinTosQueueDisc>()
            .AddAttribute("TosMap",
                          "The ToS to band mapping.",
                          TosMapValue(TosMap{{1, 2, 2, 2, 1, 2, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1}}),
                          MakeTosMapAccessor(&RoundRobinTosQueueDisc::m_tos2band),
                          MakeTosMapChecker());
    return tid;
}

RoundRobinTosQueueDisc::RoundRobinTosQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::NO_LIMITS),
        m_lastDequeuedClass(0)  // initilze the dequeued class to 0 at the creation of the prio-queue-disc
{
    NS_LOG_FUNCTION(this);
}

RoundRobinTosQueueDisc::~RoundRobinTosQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

void
RoundRobinTosQueueDisc::SetBandForToS(uint8_t tos, uint16_t band)
{
    NS_LOG_FUNCTION(this << tos << band);

    NS_ASSERT_MSG(tos < 31, "ToS must be a value between 0 and 30");

    m_tos2band[tos] = band;
}

uint16_t
RoundRobinTosQueueDisc::GetBandForToS(uint8_t tos) const
{
    NS_LOG_FUNCTION(this << tos);

    NS_ASSERT_MSG(tos < 31, "Priority must be a value between 0 and 31");

    return m_tos2band[tos];
}

bool
RoundRobinTosQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    uint32_t band = m_tos2band[0];

    int32_t ret = Classify(item);

    if (ret == PacketFilter::PF_NO_MATCH)
    {
        NS_LOG_DEBUG("No filter has been able to classify this packet, using TosMap.");

        SocketIpTosTag ipTosTag;
        if (item->GetPacket()->PeekPacketTag(ipTosTag))
        {
            std::cout << "ToS: " << int(ipTosTag.GetTos()) << std::endl;
            band = m_tos2band[(ipTosTag.GetTos() / 2) & 0x0f];  // ToS can be an EVEN number between 0 and 30 (Hex). Each ToS value corresponds to an index 0:15 on the "tos2Band"
        }
        else
        {
            std::cout << "ToS: " << 0 << std::endl;
        }
        
    }
    else
    {
        NS_LOG_DEBUG("Packet filters returned " << ret);

        if (ret >= 0 && static_cast<uint32_t>(ret) < GetNQueueDiscClasses())
        {
            band = ret;
        }
    }

    // for debug:
    // SharedPriorityTag flowPrioTag;
    // uint8_t flow_priority = 0;
    // if (item->GetPacket ()->PeekPacketTag (flowPrioTag))
    // {
    //     flow_priority = flowPrioTag.GetSimpleValue();
    // }
    // std::cout << "Packet of priority: " << int(flow_priority) << " enqueued in band: " << band << std::endl;
    //////////////
    NS_ASSERT_MSG(band < GetNQueueDiscClasses(), "Selected band out of range");
    bool retval = GetQueueDiscClass(band)->GetQueueDisc()->Enqueue(item);

    // If Queue::Enqueue fails, QueueDisc::Drop is called by the child queue disc
    // because QueueDisc::AddQueueDiscClass sets the drop callback

    NS_LOG_LOGIC("Number packets band " << band << ": "
                                        << GetQueueDiscClass(band)->GetQueueDisc()->GetNPackets());

    return retval;
}

// Round-Robbin DoDequeue:
Ptr<QueueDiscItem>
RoundRobinTosQueueDisc::DoDequeue()
{
    Ptr<QueueDiscItem> item;

    uint8_t nextQueue = m_lastDequeuedClass + 1;
    if (nextQueue >= GetNQueueDiscClasses()) 
    {
        nextQueue = 0;
    }

    for (uint8_t i = 0; i < GetNQueueDiscClasses(); i++) 
    {
        uint8_t queueIndex = (nextQueue + i) % GetNQueueDiscClasses();
        if ((item = GetQueueDiscClass(queueIndex)->GetQueueDisc()->Dequeue()))
        // if (!GetQueueDiscClass(queueIndex)->GetQueueDisc()->Peek() == 0) // check if class queue is not empty
        {
            m_lastDequeuedClass = queueIndex;
            // item = GetQueueDiscClass(queueIndex)->GetQueueDisc()->Dequeue();
            NS_LOG_LOGIC("Popped from band " << queueIndex << ": " << item);
            NS_LOG_LOGIC("Number of packets in band "
                        << queueIndex << ": " << GetQueueDiscClass(queueIndex)->GetQueueDisc()->GetNPackets());
            // for debug:
            std::cout << "Packet dequeued from band: " << int(queueIndex) << std::endl;
            std::cout << "Number of packets in band " << int(queueIndex) << ": " << GetQueueDiscClass(queueIndex)->GetQueueDisc()->GetNPackets() << std::endl;
            //////////////
            return item;
        }
    }
    NS_LOG_LOGIC("Queue empty");
    return item;
}



Ptr<const QueueDiscItem>
RoundRobinTosQueueDisc::DoPeek()
{
    NS_LOG_FUNCTION(this);

    Ptr<const QueueDiscItem> item;

    for (uint32_t i = 0; i < GetNQueueDiscClasses(); i++)
    {
        if ((item = GetQueueDiscClass(i)->GetQueueDisc()->Peek()))
        {
            NS_LOG_LOGIC("Peeked from band " << i << ": " << item);
            NS_LOG_LOGIC("Number packets band "
                         << i << ": " << GetQueueDiscClass(i)->GetQueueDisc()->GetNPackets());
            return item;
        }
    }

    NS_LOG_LOGIC("Queue empty");
    return item;
}

bool
RoundRobinTosQueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);
    if (GetNInternalQueues() > 0)
    {
        NS_LOG_ERROR("PrioQueueDisc cannot have internal queues");
        return false;
    }

    if (GetNQueueDiscClasses() == 0)
    {
        // create 3 fifo queue discs
        ObjectFactory factory;
        factory.SetTypeId("ns3::FifoQueueDisc");
        for (uint8_t i = 0; i < 2; i++)
        {
            Ptr<QueueDisc> qd = factory.Create<QueueDisc>();
            qd->Initialize();
            Ptr<QueueDiscClass> c = CreateObject<QueueDiscClass>();
            c->SetQueueDisc(qd);
            AddQueueDiscClass(c);
        }
    }

    if (GetNQueueDiscClasses() < 2)
    {
        NS_LOG_ERROR("PrioQueueDisc needs at least 2 classes");
        return false;
    }

    return true;
}

void
RoundRobinTosQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);
}

} // namespace ns3
