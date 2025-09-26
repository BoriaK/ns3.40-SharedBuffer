#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-socket-state.h"
#include "ns3/log.h"
#include "ns3/type-id.h"
#include <limits>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpNoCongestion");

class TcpNoCongestion : public TcpCongestionOps
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::TcpNoCongestion")
      .SetParent<TcpCongestionOps> ()
      .SetGroupName ("Internet")
      .AddConstructor<TcpNoCongestion> ();
    return tid;
  }

  TcpNoCongestion () {}
  ~TcpNoCongestion () override {}

  // required virtuals from TcpCongestionOps
  std::string GetName() const override { return "TcpNoCongestion"; }

  // return a copy of this congestion control object
  Ptr<TcpCongestionOps> Fork() override { return CreateObject<TcpNoCongestion> (); }

  // called to update cwnd (congestion avoidance) — no-op
  void IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override
  {
    // Implement growth like NewReno (slow start + congestion avoidance)
    // so this congestion control will grow normally but won't reduce on loss
    if (tcb->m_cWnd < tcb->m_ssThresh)
    {
      // Slow start: increase by one segment per ACK (count at most one here)
      if (segmentsAcked >= 1)
      {
        tcb->m_cWnd += tcb->m_segmentSize;
        // consume one segment
        segmentsAcked -= 1;
      }
    }

    if (tcb->m_cWnd >= tcb->m_ssThresh)
    {
      // Congestion avoidance: roughly +1 MSS per RTT
      if (segmentsAcked > 0)
      {
        double adder = static_cast<double>(tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get();
        adder = std::max(1.0, adder);
        tcb->m_cWnd += static_cast<uint32_t>(adder);
      }
    }
  }

  // optional timing-aware ACK hook — no-op
  void PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) override
  {
  // leave default behavior (no additional accounting) — growth handled in IncreaseWindow
  }

  // on loss: don't reduce cwnd — return very large ssthresh
  uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight) override
  {
    return std::numeric_limits<uint32_t>::max() / 2;
  }
};

NS_OBJECT_ENSURE_REGISTERED (TcpNoCongestion);

} // namespace ns3