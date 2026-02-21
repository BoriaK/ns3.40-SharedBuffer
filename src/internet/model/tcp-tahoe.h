/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Minimal TCP Tahoe congestion control implementation for ns-3.40.
 *
 * Tahoe behaviour on any loss event:
 *   - ssthresh = max(flightSize/2, 2*SMSS)   (same as NewReno)
 *   - cwnd     = 1 SMSS   (restart slow start — unlike NewReno's fast recovery)
 *
 * This file provides:
 *   TcpTahoe           — CongestionOps, identical AIMD to TcpNewReno.
 *   TcpTahoeRecovery   — RecoveryOps that resets cwnd=1 on entering recovery.
 *
 * Usage:
 *   Config::SetDefault("ns3::TcpL4Protocol::SocketType",
 *                       StringValue("ns3::TcpTahoe"));
 *   Config::SetDefault("ns3::TcpL4Protocol::RecoveryType",
 *                       TypeIdValue(TypeId::LookupByName("ns3::TcpTahoeRecovery")));
 */
#ifndef TCP_TAHOE_H
#define TCP_TAHOE_H

#include "tcp-congestion-ops.h"
#include "tcp-recovery-ops.h"

namespace ns3
{

/**
 * \ingroup congestionOps
 *
 * \brief TCP Tahoe congestion control algorithm.
 *
 * The window-increase rules (slow start + AIMD congestion avoidance) are
 * identical to TcpNewReno.  The TCP-Tahoe characteristic — resetting cwnd to
 * 1 on a fast-retransmit loss — is handled by TcpTahoeRecovery.
 */
class TcpTahoe : public TcpNewReno
{
  public:
    static TypeId GetTypeId();

    TcpTahoe();
    TcpTahoe(const TcpTahoe& sock);
    ~TcpTahoe() override;

    std::string GetName() const override;
    Ptr<TcpCongestionOps> Fork() override;
};

/**
 * \ingroup tcp
 *
 * \brief TCP Tahoe recovery: resets cwnd to 1 MSS on entry to recovery.
 *
 * Classic TCP Tahoe has no fast-recovery phase.  Upon three duplicate ACKs the
 * sender performs a fast retransmit and then immediately resets cwnd = 1 SMSS
 * and restarts slow start up to the new ssthresh.
 */
class TcpTahoeRecovery : public TcpRecoveryOps
{
  public:
    static TypeId GetTypeId();

    TcpTahoeRecovery();
    TcpTahoeRecovery(const TcpTahoeRecovery& sock);
    ~TcpTahoeRecovery() override;

    /**
     * Reset cwnd to 1 MSS and collapse m_cWndInfl so no window inflation
     * occurs during "recovery" (Tahoe just restarts slow start).
     */
    void EnterRecovery(Ptr<TcpSocketState> tcb,
                       uint32_t dupAckCount,
                       uint32_t unAckDataCount,
                       uint32_t deliveredBytes) override;

    /** No window inflation during Tahoe recovery. */
    void DoRecovery(Ptr<TcpSocketState> tcb, uint32_t deliveredBytes) override;

    /** Deflate cWndInfl back to m_ssThresh on exit (belt-and-suspenders). */
    void ExitRecovery(Ptr<TcpSocketState> tcb) override;

    std::string GetName() const override;
    Ptr<TcpRecoveryOps> Fork() override;
};

} // namespace ns3

#endif // TCP_TAHOE_H
