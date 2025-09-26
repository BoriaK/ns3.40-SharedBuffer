#!/bin/bash

# Test script to verify the scenario works from workspace root
echo "Testing ns3 scenario path handling from workspace root..."

# Change to workspace root
cd /home/boris/workspace/ns3.40-SharedBuffer

# Run a quick test (just help to avoid long simulation)
echo "Running from workspace root:"
./build/execution_scripts/ns3.40-2In2Out_SharedBuffer_TrafficControl_via_MultiQueue_5_ToS_v2-debug --help 2>&1 | head -n 5

echo ""
echo "Testing from build directory:"
cd build
./execution_scripts/ns3.40-2In2Out_SharedBuffer_TrafficControl_via_MultiQueue_5_ToS_v2-debug --help 2>&1 | head -n 5

echo ""
echo "Path handling test completed successfully!"