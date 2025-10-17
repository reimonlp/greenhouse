const express = require('express');
const rateLimit = require('express-rate-limit');

// Import models
const SensorReading = require('../models/SensorReading');
const RelayState = require('../models/RelayState');
const Rule = require('../models/Rule');
const SystemLog = require('../models/SystemLog');

// Rule engine functions (will be passed from main server)
let evaluateSensorRules = async () => {};
let io = null;
let ESP32_AUTH_TOKEN = '';

// Nombres de los relays según configuración del ESP32
const RELAY_NAMES = {
  0: 'luces',
  1: 'ventilador',
  2: 'bomba',
  3: 'calefactor'
};

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
  max: parseInt(process.env.RATE_LIMIT_MAX_REQUESTS) || 10000,
  trustProxy: true,
  keyGenerator: (req) => {
    // Use the real client IP when behind proxy
    return req.ip || req.connection.remoteAddress;
  }
});

function setupApiRoutes(app, ioInstance, esp32Token, evaluateSensorRulesFn) {
  io = ioInstance;
  ESP32_AUTH_TOKEN = esp32Token;
  evaluateSensorRules = evaluateSensorRulesFn;

  // Apply rate limiting to API routes
  app.use('/api/', limiter);

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
}

module.exports = {
  setupApiRoutes
};