require('dotenv').config();
const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const mongoose = require('mongoose');
const cors = require('cors');
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
const morgan = require('morgan');

const SensorReading = require('./models/SensorReading');
const RelayState = require('./models/RelayState');
const Rule = require('./models/Rule');
const SystemLog = require('./models/SystemLog');

const { evaluateSensorRules, evaluateTimeRules } = require('./ruleEngine');
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
  console.log('✓ Cliente WebSocket conectado:', socket.id);
  
  // Enviar datos iniciales al conectar
  socket.emit('connected', { 
    message: 'Conectado al servidor Greenhouse',
    timestamp: new Date()
  });
  
  socket.on('disconnect', () => {
    console.log('✗ Cliente WebSocket desconectado:', socket.id);
  });
});

// Hacer io accesible globalmente
global.io = io;

// ====== Middleware ======
app.use(helmet());
app.use(cors({
  origin: process.env.ALLOWED_ORIGINS?.split(',') || '*'
}));
app.use(express.json());
app.use(morgan('combined'));

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
    
    res.json({
      status: 'ok',
      timestamp: new Date(),
      database: dbStatus,
      uptime: process.uptime()
    });
  } catch (error) {
    res.status(500).json({ status: 'error', error: error.message });
  }
});

// ====== Sensor Endpoints ======
app.post('/api/sensors', async (req, res) => {
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
    
    // Emitir evento WebSocket con cambio de relay
    io.emit('relay:changed', relayState);
    
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
