# DequeueCallback Investigation Summary

## Backup Created
- Original file backed up to: `traffic-control-layer.cc.backup`
- You can restore with: `cp traffic-control-layer.cc.backup traffic-control-layer.cc`

## Problem Statement
Getting spikes in "packets in queue" counts that are clearly incorrect. Need to understand why.

## Root Cause Analysis

### How DequeueCallback Works
1. **Connection Point** (ScanDevices, line ~1916):
   - Callback is connected to EVERY queue disc on EVERY device
   - `q->TraceConnectWithoutContext("Dequeue", MakeCallback(&TrafficControlLayer::DequeueCallback, this))`
   - This means the callback fires for ANY packet dequeued from ANY queue

2. **The Problem**:
   - The callback receives only a `QueueDiscItem*` - NO information about which queue disc triggered it!
   - Must infer the source device/queue from the item's `txQueueIndex`
   - **Critical Issue**: `txQueueIndex` is set at ENQUEUE time, not dequeue time
   - Device lookup logic assumes `txQueueIndex == device port index`, which may not be true

### Specific Issues Found

#### Issue #1: Device Identification
**Location**: DequeueCallback, line ~1604-1650
**Problem**: 
```cpp
uint8_t txQueueIndex = item->GetTxQueueIndex();
// ... later ...
if (devIndex == txQueueIndex) {  // BUG: This comparison may be wrong
    device = ndi.first;
}
```

**Why This Is Wrong**:
- `txQueueIndex` is the queue index within the device that ENQUEUED the packet
- Device port index is derived from device name (e.g., "switchDeviceOut0" -> port 0)
- These two indices may not match, especially if:
  - Devices are created in different order
  - Queue numbering doesn't align with device numbering
  - Multi-queue devices use internal indexing

#### Issue #2: No Sub-Queue Identification
**Location**: DequeueCallback
**Problem**: 
- For multi-queue devices, we don't identify WHICH sub-queue dequeued the packet
- We only check if the device has a root queue disc
- **Reference**: Compare with `GetNumOfHighPriorityPacketsInSharedQueue()` (line ~468-485)
  which properly iterates through sub-queues:
  ```cpp
  for (size_t j = 0; j < ndi->second.m_rootQueueDisc->GetNQueueDiscClasses(); j++) {
      Ptr<QueueDisc> qDisc = ndi->second.m_rootQueueDisc->GetQueueDiscClass(j)->GetQueueDisc();
      // ... process each sub-queue
  }
  ```

#### Issue #3: Callback Doesn't Know Source Queue
**Location**: ScanDevices, line ~1916
**Problem**:
- Using `TraceConnectWithoutContext` loses information about the source queue
- The callback has no way to know which specific QueueDisc triggered it
- Must rely on unreliable inference from the packet item

## Debug Output Added

The modified code now prints extensive debug info for every dequeue:

### What to Look For

1. **Device Matching**:
   ```
   [DequeueCallback] Scanning X devices:
     Device[0]: switchDeviceIn0
     Device[1]: switchDeviceIn1  
     Device[2]: switchDeviceOut0  <- Output ports
       Port index from name: 0
       Has root queue disc with 2 sub-queues
       MATCH FOUND! Using this device.
   ```
   - **Check**: Does the "MATCH FOUND" happen for the correct device?
   - **Check**: Is txQueueIndex matching the right port?

2. **Port Index Verification**:
   ```
   [DequeueCallback] TxQueueIndex from item: 0
   [DequeueCallback] Selected device port index: 0
   ```
   - **Check**: Does this port index make sense for the packet?
   - **Check**: Are we sometimes selecting the WRONG port?

3. **Threshold Updates**:
   ```
   [DequeueCallback] Updated m_p_trace_threshold_l_0 = 205
   ```
   - **Check**: Are these the values causing the spikes in your plots?
   - **Check**: Do threshold updates correlate with the spike times?

4. **Buffer State**:
   ```
   [DequeueCallback] Current buffer occupancy: 200 / 250
   ```
   - **Check**: Does this match expected buffer occupancy?
   - **Check**: Are threshold calculations reasonable given this occupancy?

## Recommended Fixes

### Option 1: Add Device Context to Callback (BEST)
Modify ScanDevices to use lambda capture:
```cpp
q->TraceConnectWithoutContext("Dequeue", 
    MakeBoundCallback(&TrafficControlLayer::DequeueCallbackWithContext, this, dev, portIndex));
```

### Option 2: Disable DequeueCallback Threshold Updates
If thresholds don't need to be updated on dequeue (only on enqueue), comment out the callback:
```cpp
// q->TraceConnectWithoutContext("Dequeue", MakeCallback(&TrafficControlLayer::DequeueCallback, this));
```

### Option 3: Use Different Identification Method
Instead of `txQueueIndex`, try:
- Matching against the actual queue disc pointer (if available via context)
- Using device queue state to identify which queue just changed
- Adding custom tags to track queue/device through the packet lifetime

## Testing Instructions

1. **Build**:
   ```bash
   cd /home/boris/workspace/ns-allinone-3.40/ns-3.40
   ./ns3 build
   ```

2. **Run with debug output**:
   ```bash
   ./ns3 run "your-test-scenario" 2>&1 | tee dequeue_debug.log
   ```

3. **Analyze output**:
   ```bash
   grep "\[DequeueCallback\]" dequeue_debug.log | less
   ```

4. **Look for**:
   - Device matching failures: `ERROR: Could not find matching device`
   - Unexpected port indices
   - Threshold updates at suspicious times
   - Mismatched buffer occupancy values

## Key Questions to Answer

1. **Does device matching work correctly?**
   - Are devices found successfully?
   - Do port indices match expectations?
   
2. **Does txQueueIndex correlate with device port index?**
   - Print both and compare
   - Check if there's a consistent offset or mapping issue

3. **When do spikes occur?**
   - During normal operation?
   - When specific packets dequeue?
   - After certain threshold updates?

4. **Is the callback even needed?**
   - Do thresholds MUST be updated on dequeue?
   - Or can they be updated only on enqueue (in Send())?

## References in Code

- **Device iteration pattern**: Line ~378 in `GetCurrentSharedBufferSize()`
- **Sub-queue iteration**: Line ~468 in `GetNumOfHighPriorityPacketsInSharedQueue()`
- **Queue disc classes**: Line ~707 in `GetNumOfPriorityConjestedQueuesInSharedQueue()`
- **Callback connection**: Line ~1916 in `ScanDevices()`

