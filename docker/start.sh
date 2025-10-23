#!/bin/bash

# Greenhouse Docker Entry Point
# Starts Nginx (Frontend) and Node.js Backend

set -e

echo "🚀 Starting Greenhouse System..."
echo ""

# Function to handle graceful shutdown
cleanup() {
    echo ""
    echo "⏹️  Shutting down..."
    # Kill nginx
    if ps aux | grep -q "[n]ginx"; then
        echo "Stopping Nginx..."
        killall -TERM nginx 2>/dev/null || true
    fi
    # Kill node backend
    if ps aux | grep -q "[n]ode"; then
        echo "Stopping Node.js backend..."
        killall -TERM node 2>/dev/null || true
    fi
    exit 0
}

# Set trap to catch signals
trap cleanup SIGTERM SIGINT

# Verify required files
if [ ! -f /etc/nginx/nginx.conf ]; then
    echo "❌ ERROR: Nginx config not found!"
    exit 1
fi

if [ ! -f /app/backend/package.json ]; then
    echo "❌ ERROR: Backend package.json not found!"
    exit 1
fi

if [ ! -d /www ]; then
    echo "❌ ERROR: Frontend dist directory not found!"
    exit 1
fi

echo "✅ All required files present"
echo ""

# Log configuration
echo "📋 Configuration:"
echo "   - Node Environment: $NODE_ENV"
echo "   - Port: $PORT"
echo "   - MongoDB: $MONGODB_URI"
echo ""

# Start Nginx in background
echo "🌐 Starting Nginx..."
nginx -g "daemon off;" &
NGINX_PID=$!
echo "   Nginx PID: $NGINX_PID"

# Wait for nginx to start
sleep 2

# Verify nginx started (usar kill -0 en lugar de ps -p para Alpine)
if ! kill -0 $NGINX_PID 2>/dev/null; then
    echo "❌ ERROR: Nginx failed to start"
    exit 1
fi

echo "✅ Nginx started successfully"
echo ""

# Start Node.js backend
echo "🔧 Starting Node.js Backend..."
cd /app/backend

# Run backend in foreground (PID 1)
npm start &
BACKEND_PID=$!
echo "   Backend PID: $BACKEND_PID"

# Wait for both processes
echo ""
echo "✨ System Ready"
echo "   Frontend: http://localhost:3000"
echo "   Backend API: http://localhost:8080"
echo ""

# Keep the container running by waiting for both processes
wait $NGINX_PID $BACKEND_PID

# If we get here, a process died
echo "⚠️  A process exited unexpectedly"
cleanup
exit 1