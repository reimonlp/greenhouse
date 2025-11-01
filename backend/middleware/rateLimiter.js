// WebSocket Rate Limiting Configuration
const RATE_LIMIT_WINDOW_MS = 60000; // 1 minute
const RATE_LIMIT_MAX_EVENTS = 120;  // 120 events per minute per socket

// Rate limiter storage: Map of socket.id -> { count, resetTime }
const socketRateLimits = new Map();

function checkSocketRateLimit(socket, eventName) {
  const now = Date.now();
  const socketId = socket.id;

  // Get or create rate limit data for this socket
  let limitData = socketRateLimits.get(socketId);

  if (!limitData || now > limitData.resetTime) {
    // Create new window
    limitData = {
      count: 0,
      resetTime: now + RATE_LIMIT_WINDOW_MS
    };
    socketRateLimits.set(socketId, limitData);
  }

  // Increment counter
  limitData.count++;

  // Check if limit exceeded
  if (limitData.count > RATE_LIMIT_MAX_EVENTS) {
    const timeUntilReset = Math.ceil((limitData.resetTime - now) / 1000);
    console.log(`🚨 [RATE_LIMIT] Socket ${socketId} exceeded limit on event '${eventName}' (${limitData.count}/${RATE_LIMIT_MAX_EVENTS}). Reset in ${timeUntilReset}s`);
    return false; // Rate limit exceeded
  }

  return true; // Within limits
}

// Cleanup old rate limit entries periodically
setInterval(() => {
  const now = Date.now();
  for (const [socketId, data] of socketRateLimits.entries()) {
    if (now > data.resetTime + 60000) { // Clean up 1 minute after reset
      socketRateLimits.delete(socketId);
    }
  }
}, 120000); // Clean up every 2 minutes

module.exports = {
  checkSocketRateLimit,
  socketRateLimits
};