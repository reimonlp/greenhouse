const mongoose = require('mongoose');

function setupHealthCheck(app, io, socketRateLimits) {
  // ====== Health Check ======
  app.get('/health', async (req, res) => {
    try {
      const dbState = mongoose.connection.readyState;
      const dbStatus = dbState === 1 ? 'connected' : 'disconnected';

      // Count authenticated ESP32 devices
      const authenticatedDevices = Array.from(io.sockets.sockets.values())
        .filter(socket => socket.authenticated && socket.deviceType === 'esp32')
        .map(socket => ({
          device_id: socket.deviceId,
          connected_at: socket.handshake.time,
          metrics: socket.metrics || null
        }));

      // Get memory usage
      const memUsage = process.memoryUsage();
      const memoryMB = {
        rss: Math.round(memUsage.rss / 1024 / 1024 * 100) / 100,
        heapUsed: Math.round(memUsage.heapUsed / 1024 / 1024 * 100) / 100,
        heapTotal: Math.round(memUsage.heapTotal / 1024 / 1024 * 100) / 100
      };

      // Get uptime in human-readable format
      const uptimeSeconds = Math.floor(process.uptime());
      const uptimeMinutes = Math.floor(uptimeSeconds / 60);
      const uptimeHours = Math.floor(uptimeMinutes / 60);
      const uptimeDays = Math.floor(uptimeHours / 24);

      const uptimeFormatted = `${uptimeDays}d ${uptimeHours % 24}h ${uptimeMinutes % 60}m ${uptimeSeconds % 60}s`;

      // Get rate limiting statistics
      const rateLimitStats = {
        trackedSockets: socketRateLimits.size,
        maxEventsPerMinute: 120, // From rateLimiter.js
        windowMs: 60000
      };

      res.json({
        status: 'ok',
        timestamp: new Date(),
        uptime: {
          seconds: uptimeSeconds,
          formatted: uptimeFormatted
        },
        database: {
          status: dbStatus,
          readyState: dbState // 0=disconnected, 1=connected, 2=connecting, 3=disconnecting
        },
        websocket: {
          totalConnections: io.sockets.sockets.size,
          authenticatedESP32: authenticatedDevices.length,
          devices: authenticatedDevices,
          rateLimit: rateLimitStats
        },
        memory: memoryMB,
        environment: process.env.NODE_ENV || 'development'
      });
    } catch (error) {
      console.error('❌ [ERROR] Health check failed:', error);
      res.status(500).json({ status: 'error', error: error.message });
    }
  });
}

module.exports = {
  setupHealthCheck
};