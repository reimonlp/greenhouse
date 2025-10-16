require('dotenv').config();
const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const mongoose = require('mongoose');
const cors = require('cors');
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
const morgan = require('morgan');

// ESP32 authentication token (use env variable in production)
const ESP32_AUTH_TOKEN = process.env.ESP32_AUTH_TOKEN || 'esp32_gh_prod_tk_9f8e7d6c5b4a3210fedcba9876543210abcdef1234567890';

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
  console.log('⚠️ [WARN] ruleEngine.js not found - rule evaluation disabled');
}

const app = express();
const PORT = process.env.PORT || 3000;

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

// Socket.IO connection handler
io.on('connection', (socket) => {
  // Silent connection - no log noise
  
  // Enviar datos iniciales al conectar
  socket.emit('connected', { 
    message: 'Conectado al servidor Greenhouse',
    timestamp: new Date()
  });
  
  // ====== ESP32 Device Events ======
  
  // Device registration with authentication
  socket.on('device:register', async (data) => {
    // Silent attempt - only log failures and successes
    
    // Validate authentication token
    if (!data.auth_token || data.auth_token !== ESP32_AUTH_TOKEN) {
      const realIP = socket.handshake.headers['x-forwarded-for']?.split(',')[0] || socket.handshake.address;
      console.log('🚨 [SECURITY] Authentication failed for device:', data.device_id, '| IP:', realIP);
      socket.emit('device:auth_failed', { 
        error: 'Invalid authentication token',
        device_id: data.device_id
      });
      socket.disconnect(true);
      return;
    }
    
    // Log successful authentication (important event)
    const realIP = socket.handshake.headers['x-forwarded-for']?.split(',')[0] || socket.handshake.address;
    console.log('✅ [AUTH] Device authenticated:', data.device_id, '| Firmware:', data.firmware_version, '| IP:', realIP);
    socket.deviceId = data.device_id;
    socket.deviceType = data.device_type;
    socket.authenticated = true;
    
    // Join device room for targeted messages
    socket.join('esp32_devices');
    
    // Send success confirmation
    socket.emit('device:auth_success', {
      device_id: data.device_id,
      message: 'Authentication successful'
    });
    
    // Log connection without exposing sensitive data
    const sanitizedData = {
      device_id: data.device_id,
      device_type: data.device_type,
      firmware_version: data.firmware_version,
      ip: socket.handshake.address
      // auth_token deliberately excluded for security
    };
    
    await SystemLog.create({
      level: 'info',
      message: `ESP32 device ${data.device_id} connected via WebSocket`,
      metadata: sanitizedData
    });
  });
  
  // Sensor data from ESP32
  socket.on('sensor:data', async (data) => {
    // Check authentication
    if (!socket.authenticated) {
      console.log('🚨 [SECURITY] Unauthorized sensor:data attempt from:', socket.id);
      return;
    }
    
    // Silent processing - no log spam for normal sensor data
    
    try {
      const sensorReading = await SensorReading.create({
        device_id: data.device_id || 'ESP32_GREENHOUSE_01',
        temperature: data.temperature,
        humidity: data.humidity,
        soil_moisture: data.soil_moisture
      });
      
      // Broadcast to all connected clients (dashboard)
      io.emit('sensor:new', sensorReading);
      
      // Evaluate sensor-based rules (will log if thresholds exceeded)
      await evaluateSensorRules(sensorReading);
    } catch (error) {
      console.error('❌ [ERROR] Failed to save sensor data:', error.message);
    }
  });
  
  // Relay state update from ESP32
  socket.on('relay:state', async (data) => {
    // Check authentication
    if (!socket.authenticated) {
      console.log('🚨 [SECURITY] Unauthorized relay:state attempt from:', socket.id);
      return;
    }
    
    // Log relay changes (important event)
    const relayNames = ['Luces', 'Ventilador', 'Bomba', 'Calefactor'];
    const relayName = relayNames[data.relay_id] || `Relay ${data.relay_id}`;
    console.log(`🔌 [RELAY] ${relayName} → ${data.state ? 'ON' : 'OFF'} | Mode: ${data.mode || 'manual'} | By: ${data.changed_by || 'esp32'}`);
    
    try {
      const relayState = await RelayState.findOneAndUpdate(
        { relay_id: data.relay_id },
        {
          state: data.state,
          mode: data.mode || 'manual',
          changed_by: data.changed_by || 'esp32',
          timestamp: new Date()
        },
        { upsert: true, new: true }
      );
      
      // Broadcast to all connected clients (dashboard)
      io.emit('relay:changed', relayState);
      
      // Log to SystemLog for audit trail
      await SystemLog.create({
        level: 'info',
        message: `Relay ${data.relay_id} (${relayName}) changed to ${data.state ? 'ON' : 'OFF'}`,
        metadata: {
          relay_id: data.relay_id,
          relay_name: relayName,
          state: data.state,
          mode: data.mode || 'manual',
          changed_by: data.changed_by || 'esp32'
        }
      });
    } catch (error) {
      console.error('❌ [ERROR] Failed to update relay state:', error.message);
    }
  });
  
  // Log from ESP32
  socket.on('log', async (data) => {
    // Only log warnings and errors from ESP32
    if (data.level === 'warn' || data.level === 'error') {
      console.log(`⚠️ [ESP32-${data.level.toUpperCase()}]`, data.message);
    }
    
    try {
      await SystemLog.create({
        level: data.level || 'info',
        message: data.message,
        metadata: { device_id: data.device_id }
      });
    } catch (error) {
      console.error('❌ [ERROR] Failed to save ESP32 log:', error.message);
    }
  });
  
  // Ping/Pong for keepalive
  socket.on('ping', (data) => {
    // Silent ping/pong - no log spam
    socket.emit('pong', { timestamp: new Date() });
  });
  
  socket.on('disconnect', () => {
    // Only log ESP32 device disconnections (important)
    if (socket.deviceId) {
      console.log('⚠️ [DISCONNECT] ESP32 device disconnected:', socket.deviceId);
    }
    // Silent disconnect for dashboard clients
  });
});

// Hacer io accesible globalmente
global.io = io;

// ====== Middleware ======
// Trust proxy - necesario para obtener IP real detrás de nginx
app.set('trust proxy', true);

app.use(helmet());
app.use(cors({
  origin: process.env.ALLOWED_ORIGINS?.split(',') || '*'
}));
app.use(express.json());

// Custom morgan format to log real IP addresses
morgan.token('real-ip', (req) => {
  return req.ip || req.connection.remoteAddress;
});
app.use(morgan(':real-ip - :method :url :status :res[content-length] - :response-time ms'));

// ESP32 authentication middleware for API routes
const authenticateESP32 = (req, res, next) => {
  const authHeader = req.headers.authorization;
  
  if (!authHeader || !authHeader.startsWith('Bearer ')) {
    return res.status(401).json({ 
      success: false, 
      error: 'Missing or invalid Authorization header' 
    });
  }
  
  const token = authHeader.substring(7); // Remove "Bearer " prefix
  
  if (token !== ESP32_AUTH_TOKEN) {
    return res.status(403).json({ 
      success: false, 
      error: 'Invalid authentication token' 
    });
  }
  
  // Token is valid
  next();
};

// Rate limiting
const limiter = rateLimit({
  windowMs: parseInt(process.env.RATE_LIMIT_WINDOW_MS) || 15 * 60 * 1000,
  max: parseInt(process.env.RATE_LIMIT_MAX_REQUESTS) || 10000
});
app.use('/api/', limiter);

// ====== MongoDB Connection ======
mongoose.connect(process.env.MONGODB_URI, {
  useNewUrlParser: true,
  useUnifiedTopology: true
})
.then(() => console.log('✓ MongoDB conectado'))
.catch(err => {
  console.error('✗ Error conectando MongoDB:', err);
  process.exit(1);
});

// ====== Time-based Rules Scheduler ======
setInterval(async () => {
  await evaluateTimeRules();
}, 60000); // Cada minuto

console.log('✓ Time-based rules scheduler started (checks every minute)');

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
        connected_at: socket.handshake.time
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
        devices: authenticatedDevices
      },
      memory: memoryMB,
      environment: process.env.NODE_ENV || 'development'
    });
  } catch (error) {
    console.error('❌ [ERROR] Health check failed:', error);
    res.status(500).json({ status: 'error', error: error.message });
  }
});

// ====== Sensor Endpoints ======
app.post('/api/sensors', authenticateESP32, async (req, res) => {
  try {
    const reading = new SensorReading(req.body);
    await reading.save();
    
    // Emitir evento WebSocket con nueva lectura
    io.emit('sensor:new', reading);
    
    await evaluateSensorRules(reading);
    
    res.status(201).json({ success: true, data: reading });
  } catch (error) {
    console.error('Error saving sensor reading:', error);
    res.status(400).json({ success: false, error: error.message });
  }
});

app.get('/api/sensors', async (req, res) => {
  try {
    const limit = parseInt(req.query.limit) || 100;
    const readings = await SensorReading.find()
      .sort({ timestamp: -1 })
      .limit(limit);
    res.json({ success: true, data: readings });
  } catch (error) {
    res.status(500).json({ success: false, error: error.message });
  }
});

app.get('/api/sensors/latest', async (req, res) => {
  try {
    const latest = await SensorReading.findOne().sort({ timestamp: -1 });
    res.json({ success: true, data: latest });
  } catch (error) {
    res.status(500).json({ success: false, error: error.message });
  }
});

// ====== Relay Endpoints ======
// Nombres de los relays según configuración del ESP32
const RELAY_NAMES = {
  0: 'luces',
  1: 'ventilador',
  2: 'bomba',
  3: 'calefactor'
};

app.get('/api/relays/states', async (req, res) => {
  try {
    // Obtener el último estado de cada relay usando aggregation
    const states = await RelayState.aggregate([
      { $sort: { relay_id: 1, timestamp: -1 } },
      {
        $group: {
          _id: '$relay_id',
          latestState: { $first: '$$ROOT' }
        }
      },
      { $replaceRoot: { newRoot: '$latestState' } },
      { $sort: { relay_id: 1 } }
    ]);
    
    // Agregar nombres a cada relay
    const statesWithNames = states.map(state => ({
      ...state,
      name: RELAY_NAMES[state.relay_id] || `relay_${state.relay_id}`
    }));
    
    res.json({ success: true, data: statesWithNames });
  } catch (error) {
    res.status(500).json({ success: false, error: error.message });
  }
});

app.post('/api/relays/:id/state', async (req, res) => {
  try {
    const { id } = req.params;
    const { state, manual = false } = req.body;
    
    const relayState = await RelayState.findOneAndUpdate(
      { relay_id: parseInt(id) },
      { 
        state,
        manual,
        last_change: new Date()
      },
      { upsert: true, new: true }
    );
    
    // Emitir evento WebSocket con cambio de relay a dashboard
    io.emit('relay:changed', relayState);
    
    // Enviar comando al ESP32 via WebSocket
    io.to('esp32_devices').emit('relay:command', {
      relay_id: parseInt(id),
      state: state
    });
    
    console.log(`🔌 Relay command sent to ESP32: Relay ${id} -> ${state ? 'ON' : 'OFF'}`);
    
    await SystemLog.create({
      level: 'info',
      message: `Relay ${id} ${state ? 'activado' : 'desactivado'} ${manual ? '(manual)' : '(automático)'}`,
      metadata: { relay_id: id, state, manual }
    });
    
    res.json({ success: true, data: relayState });
  } catch (error) {
    res.status(500).json({ success: false, error: error.message });
  }
});

// ====== Rules Endpoints ======
app.get('/api/rules', async (req, res) => {
  try {
    const rules = await Rule.find().sort({ priority: -1 });
    res.json({ success: true, data: rules });
  } catch (error) {
    res.status(500).json({ success: false, error: error.message });
  }
});

app.post('/api/rules', async (req, res) => {
  try {
    const rule = new Rule(req.body);
    await rule.save();
    
    // Emitir evento WebSocket con nueva regla
    io.emit('rule:created', rule);
    
    res.status(201).json({ success: true, data: rule });
  } catch (error) {
    res.status(400).json({ success: false, error: error.message });
  }
});

app.put('/api/rules/:id', async (req, res) => {
  try {
    const rule = await Rule.findByIdAndUpdate(req.params.id, req.body, { new: true });
    
    // Emitir evento WebSocket con regla actualizada
    io.emit('rule:updated', rule);
    
    res.json({ success: true, data: rule });
  } catch (error) {
    res.status(400).json({ success: false, error: error.message });
  }
});

app.delete('/api/rules/:id', async (req, res) => {
  try {
    await Rule.findByIdAndDelete(req.params.id);
    
    // Emitir evento WebSocket con regla eliminada
    io.emit('rule:deleted', { id: req.params.id });
    
    res.json({ success: true });
  } catch (error) {
    res.status(500).json({ success: false, error: error.message });
  }
});

// ====== Logs Endpoints ======
app.get('/api/logs', async (req, res) => {
  try {
    const limit = parseInt(req.query.limit) || 50;
    const logs = await SystemLog.find()
      .sort({ timestamp: -1 })
      .limit(limit);
    res.json({ success: true, data: logs });
  } catch (error) {
    res.status(500).json({ success: false, error: error.message });
  }
});

// ====== Error Handler ======
app.use((err, req, res, next) => {
  console.error('Error no manejado:', err);
  res.status(500).json({ error: 'Error interno del servidor' });
});

// ====== Start Server ======
server.listen(PORT, () => {
  console.log(`✓ Greenhouse API corriendo en puerto ${PORT}`);
  console.log(`✓ WebSocket habilitado en /socket.io/`);
  console.log(`✓ Modo: ${process.env.NODE_ENV || 'development'}`);
  console.log(`✓ Health check: http://localhost:${PORT}/health`);
});

// Graceful shutdown
process.on('SIGTERM', () => {
  console.log('SIGTERM recibido, cerrando servidor...');
  io.close();
  mongoose.connection.close();
  process.exit(0);
});
