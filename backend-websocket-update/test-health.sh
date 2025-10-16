#!/bin/bash

# Test Health Endpoint
# Tests the improved health endpoint on the VPS

set -e

VPS="root@reimon.dev"
PORT="5591"

echo "🏥 Testing Health Endpoint..."
echo "================================"

# Test health endpoint
RESPONSE=$(ssh -p $PORT $VPS "curl -s http://localhost:3000/health")

# Parse and display key metrics
echo "$RESPONSE" | python3 -c "
import sys, json
data = json.load(sys.stdin)
print(f\"✅ Status: {data['status']}\")
print(f\"⏱️  Uptime: {data['uptime']['formatted']}\")
print(f\"💾 Database: {data['database']['status']}\")
print(f\"🔌 WebSocket Connections: {data['websocket']['totalConnections']}\")
print(f\"📡 Authenticated ESP32: {data['websocket']['authenticatedESP32']}\")
print(f\"🧠 Memory (RSS): {data['memory']['rss']} MB\")
print(f\"🧠 Memory (Heap Used): {data['memory']['heapUsed']} MB\")
print(f\"🌍 Environment: {data['environment']}\")

if data['websocket']['authenticatedESP32'] > 0:
    print(f\"\\n📱 Connected Devices:\")
    for device in data['websocket']['devices']:
        print(f\"   - {device['device_id']} (connected at {device['connected_at']})\")
"

echo ""
echo "✅ Health endpoint test completed successfully!"
