# PrioSteadyOn Application - Summary

## What Was Created

A new ns-3 application called **PrioSteadyOn** that generates continuous CBR traffic without ON/OFF periods.

### Files Created:
1. **Application Implementation**
   - `/src/applications/model/prio-steady-on-application.h` (176 lines)
   - `/src/applications/model/prio-steady-on-application.cc` (435 lines)

2. **Helper Classes**
   - `/src/applications/helper/prio-steady-on-helper.h` (103 lines)
   - `/src/applications/helper/prio-steady-on-helper.cc` (98 lines)

3. **Build Configuration**
   - Updated: `/src/applications/CMakeLists.txt` (added new source and header files)

4. **Integration**
   - Updated: `/execution_scripts/2In2Out_SharedBuffer_TrafficControl_via_MultiQueue_Predictive_1_of_2_ToS_1_port.cc`
     * Added `#include "ns3/prio-steady-on-helper.h"` (line 54)
     * Added protocol type configuration for prioSteadyOn (lines 648-655)
     * Added application installation section (lines 1092-1121)

5. **Documentation**
   - Created: `PrioSteadyOn_Usage_Guide.md` (comprehensive usage guide)

### Build Status: ✅ SUCCESS
- All files compiled successfully
- Object files generated in `/build/src/applications/`
- Headers copied to `/build/include/ns3/`
- Integrated into `libns3.40-applications-debug.so`

## Key Features

### What PrioSteadyOn Does:
✅ Generates **continuous CBR traffic** (Constant Bit Rate)
✅ Uses **scheduled packet generation** (like PrioOnOff) - smooth, predictable traffic
✅ **No ON/OFF periods** - traffic starts when app starts, stops when app stops
✅ **Application-level rate limiting** via `DataRate` attribute
✅ Supports all **priority/ToS tagging** features (FlowPriority, MiceElephantProb, ApplicationToS)
✅ Works with **TCP and UDP**
✅ **SeqTsSizeHeader** support for sequence/timestamp tracking

### What Makes It Different:

| Feature | PrioOnOff | PrioBulkSend | **PrioSteadyOn** |
|---------|-----------|--------------|------------------|
| **Traffic Pattern** | ON/OFF cycles | Continuous loop | **Continuous CBR** |
| **Rate Control** | DataRate | TCP-limited | **DataRate** |
| **Scheduling** | ScheduleNextTx() | while(Send()) | **ScheduleNextTx()** |
| **BBR Stability** | ✅ Stable | ❌ Unstable | **✅ Stable** |
| **Queue Behavior** | Smooth | Sawtooth | **Smooth** |
| **Use Case** | Bursty traffic | Bulk transfer | **Streaming** |

## How to Use

### 1. Set Application Type
```cpp
std::string applicationType = "prioSteadyOn"; // Line 476 in execution script
```

### 2. Configure Application
```cpp
PrioSteadyOnHelper clientHelper(socketType, socketAddress);
clientHelper.SetAttribute("Remote", AddressValue(socketAddress));
clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
clientHelper.SetAttribute("DataRate", StringValue("2Mb/s")); // CBR rate
clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // 0 = infinite
clientHelper.SetAttribute("FlowPriority", UintegerValue(0x2)); // Low priority
```

### 3. Build and Run
```bash
cd /home/boris/workspace/ns-allinone-3.40/ns-3.40
./ns3 build
./ns3 run "2In2Out_SharedBuffer_TrafficControl_via_MultiQueue_Predictive_1_of_2_ToS_1_port"
```

## Expected Behavior

With TCP-BBR (InitialCwnd=250, HighGain=4.0) and PrioSteadyOn:
- **Smooth queue filling**: 0 → 209 packets over ~1.5 seconds
- **Stable occupancy**: Queue stays at ~208-209 packets (dynamic threshold)
- **No sawtooth**: Unlike PrioBulkSend
- **Predictable throughput**: ~0.499 Mbps (like PrioOnOff)
- **Traffic-control drops**: Reaches dynamic threshold aggressively

## Comparison: All Three Applications

### Test Scenario: 1In1Out, 500 Kbps bottleneck, BUFFER_SIZE=250, TCP-BBR

**PrioOnOff** (with OnTime=constant, OffTime=0):
- Queue: Smooth filling, stable at ~209 packets
- Throughput: ~0.499 Mbps
- Pattern: Scheduled packet generation (4ms inter-packet gap)
- BBR: Stable bandwidth/RTT estimation

**PrioBulkSend**:
- Queue: Sawtooth (208→98→208 packets over 2 seconds)
- Throughput: ~0.187 Mbps (conservative due to instability)
- Pattern: Tight while-loop, bursty
- BBR: Unstable estimation due to burst-pause pattern

**PrioSteadyOn** (NEW):
- Queue: Smooth filling, stable at ~209 packets (expected - same as PrioOnOff)
- Throughput: ~0.499 Mbps (expected - same as PrioOnOff)
- Pattern: Scheduled packet generation (4ms inter-packet gap)
- BBR: Stable bandwidth/RTT estimation

## Why PrioSteadyOn Was Created

### Problem:
- **PrioOnOff**: Great for bursty traffic, but requires OnTime/OffTime configuration even for continuous traffic
- **PrioBulkSend**: Continuous, but tight-loop sending causes sawtooth behavior and BBR instability

### Solution:
- **PrioSteadyOn**: Combines continuous operation (like PrioBulkSend) with scheduled generation (like PrioOnOff)
- Simpler than PrioOnOff for continuous traffic (no ON/OFF state machine)
- More stable than PrioBulkSend (rate-limited, not burst-limited)

## Use Cases

### ✅ When to Use PrioSteadyOn:
- **Video streaming** (continuous CBR)
- **VoIP** (sustained audio)
- **Live sensors/telemetry** (constant data feed)
- **Steady-state testing** (analyze queue without ON/OFF transients)

### ✅ When to Use PrioOnOff:
- **Web browsing** (bursty with pauses)
- **File downloads** (ON/OFF patterns)
- **Human interaction** (natural burst patterns)

### ✅ When to Use PrioBulkSend:
- **Max throughput tests** (no rate limiting)
- **Bulk file transfers** (send as fast as possible)
- **Stress testing** (aggressive congestion)

## Next Steps

To test PrioSteadyOn:

1. **Edit execution script** (line 476):
   ```cpp
   std::string applicationType = "prioSteadyOn";
   ```

2. **Build**:
   ```bash
   ./ns3 build
   ```

3. **Run**:
   ```bash
   ./ns3 run "2In2Out_SharedBuffer_TrafficControl_via_MultiQueue_Predictive_1_of_2_ToS_1_port"
   ```

4. **Analyze results**:
   - Check queue occupancy plots: `port_0_queue_1_Low.png`
   - Compare with PrioOnOff and PrioBulkSend results
   - Verify smooth queue filling (no sawtooth)
   - Check throughput (~0.499 Mbps expected)

## Technical Details

### Architecture
Based on PrioOnOff with simplified state machine (no ON/OFF cycling).

### Key Attributes
- **DataRate**: CBR transmission rate (e.g., "2Mb/s")
- **PacketSize**: Size of each packet (bytes)
- **MaxBytes**: Total bytes limit (0 = infinite)
- **FlowPriority**: Packet priority (0x1=high, 0x2=low)
- **ApplicationToS**: IP Type of Service
- **MiceElephantProbability**: D parameter for mice/elephant classification
- **Protocol**: TcpSocketFactory or UdpSocketFactory
- **EnableSeqTsSizeHeader**: Add sequence/timestamp header

### Removed Attributes (vs PrioOnOff)
- ❌ OnTime (not needed)
- ❌ OffTime (not needed)
- ❌ m_isOn flag (always "on")
- ❌ ScheduleStartEvent()/ScheduleStopEvent() methods

## Status: ✅ COMPLETE

All files created, compiled, and integrated. Ready to use!
