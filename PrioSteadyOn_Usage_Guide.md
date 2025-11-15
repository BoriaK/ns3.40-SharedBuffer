# PrioSteadyOn Application - Usage Guide

## Overview
PrioSteadyOn is a new ns-3 application that generates continuous CBR (Constant Bit Rate) traffic without ON/OFF periods. It combines the best features of PrioOnOff (scheduled, rate-limited packet generation) with continuous operation (like a single endless ON period).

## Why PrioSteadyOn?

### Problem with PrioBulkSend
- **Bursty traffic**: Tight `while(Send())` loop sends packets as fast as TCP allows
- **Unstable BBR**: Burst-pause pattern confuses BBR's bandwidth/RTT estimation
- **Sawtooth behavior**: Queue fills rapidly, drains, fills again (208→98→208 packets)
- **Lower throughput**: BBR becomes conservative (0.187 Mbps vs 0.499 Mbps with prioOnOff)

### Comparison: PrioOnOff vs PrioBulkSend vs PrioSteadyOn

| Feature | PrioOnOff | PrioBulkSend | PrioSteadyOn |
|---------|-----------|--------------|--------------|
| Traffic Pattern | ON/OFF cycles | Continuous tight loop | Continuous CBR |
| Rate Limiting | Yes (DataRate) | No (TCP-limited) | Yes (DataRate) |
| Packet Scheduling | ScheduleNextTx() | while(Send()) | ScheduleNextTx() |
| BBR Stability | Stable | Unstable (bursty) | Stable |
| Queue Behavior | Smooth filling | Sawtooth pattern | Smooth filling |
| Use Case | Periodic traffic | Bulk transfer | Sustained streaming |

## Files Created

### Application Files
- `/src/applications/model/prio-steady-on-application.h` - Header file
- `/src/applications/model/prio-steady-on-application.cc` - Implementation
- `/src/applications/helper/prio-steady-on-helper.h` - Helper header
- `/src/applications/helper/prio-steady-on-helper.cc` - Helper implementation

### Integration
- Updated: `/src/applications/CMakeLists.txt` - Added new files to build
- Updated: `/execution_scripts/2In2Out_SharedBuffer_TrafficControl_via_MultiQueue_Predictive_1_of_2_ToS_1_port.cc` - Added support for prioSteadyOn

## How to Use

### 1. Set Application Type
In your execution script (e.g., `2In2Out_SharedBuffer_TrafficControl_via_MultiQueue_Predictive_1_of_2_ToS_1_port.cc`):

```cpp
std::string applicationType = "prioSteadyOn"; // Line 476
```

### 2. Application Attributes
The PrioSteadyOn application supports the following attributes:

```cpp
PrioSteadyOnHelper clientHelper(socketType, socketAddress);

// Required attributes
clientHelper.SetAttribute("Remote", AddressValue(socketAddress));
clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
clientHelper.SetAttribute("DataRate", StringValue("2Mb/s")); // CBR rate

// Optional attributes
clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // 0 = infinite
clientHelper.SetAttribute("FlowPriority", UintegerValue(0x2)); // 0x1=high, 0x2=low
clientHelper.SetAttribute("ApplicationToS", UintegerValue(ipTos_LP));
clientHelper.SetAttribute("MiceElephantProbability", StringValue("0.5"));
clientHelper.SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
clientHelper.SetAttribute("EnableSeqTsSizeHeader", BooleanValue(false));
```

### 3. Key Differences from PrioOnOff
**Removed attributes** (not needed for continuous traffic):
- `OnTime` - No ON/OFF cycling
- `OffTime` - No OFF periods

**Same as PrioOnOff**:
- `DataRate` - Rate limiting (scheduled packet generation)
- `PacketSize` - Packet size
- `MaxBytes` - Total bytes limit
- All priority/tagging attributes

### 4. Expected Behavior

With TCP-BBR (InitialCwnd=250, HighGain=4.0):
- **Smooth queue filling**: Like prioOnOff, gradual filling from 0→209 packets over ~1.5s
- **Stable occupancy**: Queue stays at ~208-209 packets (dynamic threshold)
- **No sawtooth**: Unlike prioBulkSend, no sudden draining
- **Predictable throughput**: Similar to prioOnOff (~0.499 Mbps vs 0.187 Mbps for prioBulkSend)

## Testing

### Build the project
```bash
cd /home/boris/workspace/ns-allinone-3.40/ns-3.40
./ns3 build
```

### Run with PrioSteadyOn
```bash
# Edit line 476 in execution script:
# std::string applicationType = "prioSteadyOn";

./ns3 run "2In2Out_SharedBuffer_TrafficControl_via_MultiQueue_Predictive_1_of_2_ToS_1_port"
```

### Compare Applications
Test all three applications with identical TCP-BBR configuration:

```bash
# Test 1: prioOnOff (baseline - smooth, stable)
# applicationType = "prioOnOff"
./ns3 run ...

# Test 2: prioBulkSend (bursty, sawtooth behavior)
# applicationType = "prioBulkSend"
./ns3 run ...

# Test 3: prioSteadyOn (smooth continuous, like prioOnOff without ON/OFF)
# applicationType = "prioSteadyOn"
./ns3 run ...
```

## Implementation Details

### Architecture
PrioSteadyOn is based on PrioOnOff with simplified state machine:

**PrioOnOff state machine:**
```
StartApplication() → ScheduleStartEvent() → StartSending() → ScheduleNextTx() → SendPacket()
                                         ↓                                          ↑
                                    ScheduleStopEvent() → StopSending() ───────────┘
```

**PrioSteadyOn state machine:**
```
StartApplication() → ScheduleNextTx() → SendPacket() → ScheduleNextTx()
                           ↑                                    ↓
                           └────────────────────────────────────┘
```

### Key Methods

**`StartApplication()`**
- Creates socket
- Connects to peer
- Directly calls `ScheduleNextTx()` (no ON/OFF state)

**`ScheduleNextTx()`**
- Calculates inter-packet delay: `time = (packet_size_bits - residual_bits) / DataRate`
- Schedules next `SendPacket()` event
- Respects `MaxBytes` limit

**`SendPacket()`**
- Creates packet with SeqTsSizeHeader (optional)
- Adds priority tags (FlowPriority, MiceElephantProb, ApplicationToS)
- Sends packet via socket
- Calls `ScheduleNextTx()` to continue

**`ConnectionSucceeded()`**
- Sets `m_connected = true`
- Immediately starts transmission with `ScheduleNextTx()`

## TCP-BBR Configuration

PrioSteadyOn should work with the same TCP-BBR configuration as PrioOnOff:

```cpp
Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(250)); // Aggressive start
Config::SetDefault("ns3::TcpBbr::HighGain", DoubleValue(4.0)); // Moderate STARTUP gain
Config::SetDefault("ns3::TcpBbr::BwWindowLength", UintegerValue(10)); // Stable BW estimation
Config::SetDefault("ns3::TcpBbr::RttWindowLength", TimeValue(Seconds(10))); // Long RTT window
Config::SetDefault("ns3::TcpBbr::ProbeRttDuration", TimeValue(MilliSeconds(200)));
```

This configuration provides:
1. **Fast buffer filling** (InitialCwnd=250 → queue fills in ~1.5s)
2. **Stable operation** (long estimation windows → no sawtooth)
3. **Traffic-control drops** (aggressive enough to reach dynamic threshold ~209 packets)

## Use Cases

### When to use PrioSteadyOn
✅ **Continuous video streaming** - Sustained CBR traffic without breaks
✅ **VoIP calls** - Constant rate, no ON/OFF periods
✅ **Live data feeds** - Continuous sensor/telemetry data
✅ **Testing steady-state queue behavior** - Analyze queue dynamics without ON/OFF transients

### When to use PrioOnOff
✅ **Bursty applications** - Web browsing, file downloads with pauses
✅ **Traffic with natural ON/OFF patterns** - Human interaction patterns
✅ **Testing queue dynamics with varying load** - Analyze filling/draining cycles

### When to use PrioBulkSend
✅ **Maximum throughput testing** - Send as fast as TCP allows
✅ **File transfers** - Bulk data transfer without rate limiting
✅ **Aggressive congestion testing** - Stress test queue/congestion control

## Summary

PrioSteadyOn provides:
- ✅ **Continuous traffic** (no ON/OFF cycling)
- ✅ **Rate-limited CBR** (application-level pacing)
- ✅ **Stable BBR operation** (smooth traffic → stable estimation)
- ✅ **Predictable queue behavior** (like prioOnOff continuous ON)
- ✅ **Simple configuration** (no OnTime/OffTime attributes)

It's the ideal choice when you want continuous smooth traffic generation without the complexity of ON/OFF state management or the instability of tight-loop bulk sending.
