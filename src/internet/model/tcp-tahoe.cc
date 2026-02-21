/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "tcp-tahoe.h"
#include "tcp-socket-state.h"

#include "ns3/log.h"
#include "ns3/object.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TcpTahoe");

// ──────────────────────────────────────────────────────────────
// TcpTahoe   (congestion ops — same AIMD as TcpNewReno)
// ──────────────────────────────────────────────────────────────

NS_OBJECT_ENSURE_REGISTERED(TcpTahoe);

TypeId
TcpTahoe::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TcpTahoe")
                            .SetParent<TcpNewReno>()
                            .SetGroupName("Internet")
                            .AddConstructor<TcpTahoe>();
    return tid;
}

TcpTahoe::TcpTahoe()
    : TcpNewReno()
{
    NS_LOG_FUNCTION(this);
}

TcpTahoe::TcpTahoe(const TcpTahoe& sock)
    : TcpNewReno(sock)
{
    NS_LOG_FUNCTION(this);
}

TcpTahoe::~TcpTahoe()
{
    NS_LOG_FUNCTION(this);
}

std::string
TcpTahoe::GetName() const
{
    return "TcpTahoe";
}

Ptr<TcpCongestionOps>
TcpTahoe::Fork()
{
    return CopyObject<TcpTahoe>(this);
}

// ──────────────────────────────────────────────────────────────
// TcpTahoeRecovery   (recovery ops — cwnd=1 on any loss)
// ──────────────────────────────────────────────────────────────

NS_OBJECT_ENSURE_REGISTERED(TcpTahoeRecovery);

TypeId
TcpTahoeRecovery::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TcpTahoeRecovery")
                            .SetParent<TcpRecoveryOps>()
                            .SetGroupName("Internet")
                            .AddConstructor<TcpTahoeRecovery>();
    return tid;
}

TcpTahoeRecovery::TcpTahoeRecovery()
    : TcpRecoveryOps()
{
    NS_LOG_FUNCTION(this);
}

TcpTahoeRecovery::TcpTahoeRecovery(const TcpTahoeRecovery& sock)
    : TcpRecoveryOps(sock)
{
    NS_LOG_FUNCTION(this);
}

TcpTahoeRecovery::~TcpTahoeRecovery()
{
    NS_LOG_FUNCTION(this);
}

void
TcpTahoeRecovery::EnterRecovery(Ptr<TcpSocketState> tcb,
                                uint32_t dupAckCount [[maybe_unused]],
                                uint32_t unAckDataCount [[maybe_unused]],
                                uint32_t deliveredBytes [[maybe_unused]])
{
    NS_LOG_FUNCTION(this << tcb);
    // Tahoe: reset cwnd to 1 SMSS and restart slow start.
    // (ssthresh has already been updated by the congestion-ops GetSsThresh call
    //  before this function is invoked.)
    tcb->m_cWnd     = tcb->m_segmentSize; // cwnd = 1 MSS
    tcb->m_cWndInfl = tcb->m_segmentSize; // no ACK inflation during "recovery"
}

void
TcpTahoeRecovery::DoRecovery(Ptr<TcpSocketState> tcb [[maybe_unused]],
                              uint32_t deliveredBytes [[maybe_unused]])
{
    NS_LOG_FUNCTION(this << tcb);
    // Tahoe has no fast-recovery window inflation — do nothing.
}

void
TcpTahoeRecovery::ExitRecovery(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    // Deflate cWndInfl to the current ssthresh (belt-and-suspenders).
    tcb->m_cWndInfl = tcb->m_ssThresh.Get();
}

std::string
TcpTahoeRecovery::GetName() const
{
    return "TcpTahoeRecovery";
}

Ptr<TcpRecoveryOps>
TcpTahoeRecovery::Fork()
{
    return CopyObject<TcpTahoeRecovery>(this);
}

} // namespace ns3
