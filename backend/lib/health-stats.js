/**
 * Health Check Statistics
 * Helper functions to gather system and app statistics
 */

const mongoose = require('mongoose');

function getDatabaseStatus() {
  const dbState = mongoose.connection.readyState;
  return {
    status: dbState === 1 ? 'connected' : 'disconnected',
    readyState: dbState // 0=disconnected, 1=connected, 2=connecting, 3=disconnecting
  };
}

function getMemoryStats() {
  const memUsage = process.memoryUsage();
  return {
    rss: Math.round(memUsage.rss / 1024 / 1024 * 100) / 100,
    heapUsed: Math.round(memUsage.heapUsed / 1024 / 1024 * 100) / 100,
    heapTotal: Math.round(memUsage.heapTotal / 1024 / 1024 * 100) / 100
  };
}

function getUptimeFormatted() {
  const seconds = Math.floor(process.uptime());
  const minutes = Math.floor(seconds / 60);
  const hours = Math.floor(minutes / 60);
  const days = Math.floor(hours / 24);

  return {
    seconds,
    formatted: `${days}d ${hours % 24}h ${minutes % 60}m ${seconds % 60}s`
  };
}

function getWebSocketStats(io, socketRateLimits) {
  const authenticatedDevices = Array.from(io.sockets.sockets.values())
    .filter(socket => socket.authenticated && socket.deviceType === 'esp32')
    .map(socket => ({
      device_id: socket.deviceId,
      connected_at: socket.handshake.time,
      metrics: socket.metrics || null
    }));

  return {
    totalConnections: io.sockets.sockets.size,
    authenticatedESP32: authenticatedDevices.length,
    devices: authenticatedDevices,
    rateLimit: {
      trackedSockets: socketRateLimits.size,
      maxEventsPerMinute: 120,
      windowMs: 60000
    }
  };
}

module.exports = {
  getDatabaseStatus,
  getMemoryStats,
  getUptimeFormatted,
  getWebSocketStats
};
