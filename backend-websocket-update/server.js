require('dotenv').config();
const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const cors = require('cors');
const helmet = require('helmet');
const morgan = require('morgan');

// Import modules
const { checkSocketRateLimit, socketRateLimits } = require('./middleware/rateLimiter');
const { setupSocketHandlers } = require('./sockets/socketHandlers');
const { setupApiRoutes } = require('./routes/api');
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

// Silent token loading - no log noise

const SensorReading = require('./models/SensorReading');
const RelayState = require('./models/RelayState');
const Rule = require('./models/Rule');
const SystemLog = require('./models/SystemLog');

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

// ====== Static Frontend Files ======
// Serve static files from the frontend build directory
const path = require('path');
const frontendPath = path.join(__dirname, '../frontend/dist');

// Serve static files (CSS, JS, images, etc.)
app.use('/assets', express.static(path.join(frontendPath, 'assets')));
app.use('/favicon.svg', express.static(path.join(frontendPath, 'favicon.svg')));

// Serve the main React app for all non-API routes
app.get('*', (req, res, next) => {
  // Skip API routes
  if (req.path.startsWith('/api/') || req.path.startsWith('/socket.io/')) {
    return next();
  }
  
  // Serve index.html for all other routes (React Router)
  res.sendFile(path.join(frontendPath, 'index.html'));
});

// ====== Middleware ======
app.use(helmet());
app.use(cors({
  origin: process.env.ALLOWED_ORIGINS?.split(',') || '*'
}));
app.use(express.json());

// ====== Crear servidor HTTP ======
const server = http.createServer(app);

// ====== Configurar Socket.IO ======
const io = new Server(server, {
  cors: {
    origin: process.env.ALLOWED_ORIGINS?.split(',') || "*",
    methods: ["GET", "POST"],
    credentials: true
  },
  path: '/socket.io/'
});

// Custom morgan format to log real IP addresses
morgan.token('real-ip', (req) => {
  return req.ip || req.connection.remoteAddress;
});
app.use(morgan(':real-ip - :method :url :status :res[content-length] - :response-time ms'));

// Trust proxy - necesario para obtener IP real detrás de nginx
app.set('trust proxy', true);

// ====== Setup Socket Handlers ======
setupSocketHandlers(io, ESP32_AUTH_TOKEN, evaluateSensorRules);

// ====== Setup API Routes ======
setupApiRoutes(app, io, ESP32_AUTH_TOKEN, evaluateSensorRules);

// ====== Setup Health Check ======
setupHealthCheck(app, io, socketRateLimits);

// ====== Connect to Database ======
connectDatabase();

// ====== Time-based Rules Scheduler ======
setInterval(async () => {
  await evaluateTimeRules();
}, 60000); // Cada minuto

// Silent scheduler start - no log noise

// ====== Start Server ======
server.listen(PORT, () => {
  // Silent server start - no log noise
});

// Graceful shutdown
process.on('SIGTERM', () => {
  console.log('SIGTERM recibido, cerrando servidor...');
  io.close();
  require('mongoose').connection.close();
  process.exit(0);
});
