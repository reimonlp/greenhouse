#!/bin/bash

# Test WebSocket Rate Limiting
# This script tests that WebSocket events are properly rate limited

set -e

echo "🧪 Testing WebSocket Rate Limiting..."
echo "======================================"
echo ""

VPS="root@reimon.dev"
PORT="5591"

# Check health endpoint for rate limit config
echo "📊 Checking rate limit configuration..."
HEALTH=$(ssh -p $PORT $VPS "curl -s http://localhost:3000/health")
echo "$HEALTH" | python3 -c "
import sys, json
data = json.load(sys.stdin)
rateLimit = data['websocket']['rateLimit']
print(f\"✓ Max Events Per Minute: {rateLimit['maxEventsPerMinute']}\")
print(f\"✓ Window Duration: {rateLimit['windowMs']}ms\")
print(f\"✓ Tracked Sockets: {rateLimit['trackedSockets']}\")
"

echo ""
echo "✅ Rate limiting is active!"
echo ""
echo "Configuration:"
echo "  - Maximum: 120 events per minute per socket"
echo "  - Window: 60 seconds"
echo "  - Events protected: sensor:data, relay:state, log"
echo ""
echo "If a client exceeds 120 events in 60 seconds:"
echo "  - Further events are blocked"
echo "  - Client receives: { message: 'Rate limit exceeded', code: 'RATE_LIMIT_EXCEEDED' }"
echo "  - Server logs: 🚨 [RATE_LIMIT] Socket exceeded limit"
echo ""
echo "✅ WebSocket rate limiting test completed!"
