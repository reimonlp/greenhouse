require('dotenv').config();
const express = require('express');

// Import modules
const { setupCorsAndSecurity } = require('./middleware/cors-security');
const { setupFrontendRoutes } = require('./middleware/frontend-routes');
const { createServer, setupSocketIO, startServer, setupGracefulShutdown } = require('./lib/server-setup');
const { checkSocketRateLimit, socketRateLimits } = require('./middleware/rateLimiter');
const { setupSocketHandlers } = require('./sockets/socketHandlers');
const { setupHealthCheck } = require('./routes/health');
const { connectDatabase } = require('./config/database');

// ESP32 authentication token - MUST be set in environment variables
const ESP32_AUTH_TOKEN = process.env.ESP32_AUTH_TOKEN;

if (!ESP32_AUTH_TOKEN || ESP32_AUTH_TOKEN.length < 32) {
    console.error('❌ FATAL: ESP32_AUTH_TOKEN not set or too short (minimum 32 characters)');
    console.error('   Set it in .env file or environment: ESP32_AUTH_TOKEN=your_secure_token_here');
    console.error('   Generate a secure token: openssl rand -hex 32');
    process.exit(1);
}

// Rule engine functions (if ruleEngine.js exists)
let evaluateSensorRules = async () => {};
let evaluateTimeRules = async () => {};
try {
  const ruleEngine = require('./ruleEngine');
  evaluateSensorRules = ruleEngine.evaluateSensorRules || evaluateSensorRules;
  evaluateTimeRules = ruleEngine.evaluateTimeRules || evaluateTimeRules;
} catch (err) {
  // Silent warning - rule evaluation disabled if ruleEngine.js not found
}

const app = express();
const PORT = process.env.PORT || 3000;

// ====== Setup Middleware ======
setupCorsAndSecurity(app);

// ====== Create HTTP Server ======
const server = createServer(app);

// ====== Setup Socket.IO ======
const io = setupSocketIO(server);

// ====== Setup Routes & Handlers ======
setupSocketHandlers(io, ESP32_AUTH_TOKEN, evaluateSensorRules);
setupHealthCheck(app, io, socketRateLimits);
setupFrontendRoutes(app);

// ====== Connect to Database ======
connectDatabase();

// ====== Time-based Rules Scheduler ======
setInterval(async () => {
  await evaluateTimeRules();
}, 60000); // Every minute

// ====== Start Server ======
startServer(server, PORT);

// ====== Graceful Shutdown ======
setupGracefulShutdown(server, io);
