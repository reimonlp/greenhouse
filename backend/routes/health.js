const {
  getDatabaseStatus,
  getMemoryStats,
  getUptimeFormatted,
  getWebSocketStats
} = require('../lib/health-stats');

function setupHealthCheck(app, io, socketRateLimits) {
  // ====== Health Check Endpoint ======
  app.get('/health', async (req, res) => {
    try {
      const uptime = getUptimeFormatted();
      const database = getDatabaseStatus();
      const memory = getMemoryStats();
      const websocket = getWebSocketStats(io, socketRateLimits);

      res.json({
        status: 'ok',
        timestamp: new Date(),
        uptime,
        database,
        websocket,
        memory,
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