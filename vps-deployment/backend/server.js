require('dotenv').config();
const express = require('express');
const mongoose = require('mongoose');
const cors = require('cors');
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
const morgan = require('morgan');

const SensorReading = require('./models/SensorReading');
const RelayState = require('./models/RelayState');
const Rule = require('./models/Rule');
const SystemLog = require('./models/SystemLog');

const app = express();
const PORT = process.env.PORT || 3000;

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
  max: parseInt(process.env.RATE_LIMIT_MAX_REQUESTS) || 100
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

// ====== Health Check ======
app.get('/health', (req, res) => {
  res.json({
    status: 'ok',
    timestamp: new Date().toISOString(),
    uptime: process.uptime(),
    mongodb: mongoose.connection.readyState === 1 ? 'connected' : 'disconnected'
  });
});

// ====== API Routes ======

// POST /api/sensors - ESP32 envía lecturas de sensores
app.post('/api/sensors', async (req, res) => {
  try {
    const { device_id, temperature, humidity, soil_moisture } = req.body;
    
    if (temperature === undefined || humidity === undefined) {
      return res.status(400).json({ error: 'temperature y humidity son requeridos' });
    }

    const reading = new SensorReading({
      device_id: device_id || 'ESP32_MAIN',
      temperature,
      humidity,
      soil_moisture
    });

    await reading.save();
    
    // Log del sistema
    await SystemLog.create({
      level: 'info',
      source: 'esp32',
      message: `Sensor data received: T=${temperature}°C, H=${humidity}%`
    });

    res.status(201).json({ success: true, data: reading });
  } catch (error) {
    console.error('Error guardando sensor reading:', error);
    res.status(500).json({ error: 'Error interno del servidor' });
  }
});

// GET /api/sensors - Obtener últimas lecturas
app.get('/api/sensors', async (req, res) => {
  try {
    const limit = parseInt(req.query.limit) || 100;
    const device_id = req.query.device_id;

    const query = device_id ? { device_id } : {};
    const readings = await SensorReading
      .find(query)
      .sort({ timestamp: -1 })
      .limit(limit);

    res.json({ success: true, data: readings, count: readings.length });
  } catch (error) {
    console.error('Error obteniendo sensor readings:', error);
    res.status(500).json({ error: 'Error interno del servidor' });
  }
});

// GET /api/sensors/latest - Última lectura de cada sensor
app.get('/api/sensors/latest', async (req, res) => {
  try {
    const latest = await SensorReading
      .findOne()
      .sort({ timestamp: -1 });

    if (!latest) {
      return res.status(404).json({ error: 'No hay datos disponibles' });
    }

    res.json({ success: true, data: latest });
  } catch (error) {
    console.error('Error obteniendo última lectura:', error);
    res.status(500).json({ error: 'Error interno del servidor' });
  }
});

// POST /api/relays/:id/state - Cambiar estado de un relay
app.post('/api/relays/:id/state', async (req, res) => {
  try {
    const relay_id = parseInt(req.params.id);
    const { state, mode, changed_by } = req.body;

    if (relay_id < 0 || relay_id > 3) {
      return res.status(400).json({ error: 'relay_id debe estar entre 0 y 3' });
    }

    if (state === undefined) {
      return res.status(400).json({ error: 'state es requerido' });
    }

    const relayState = new RelayState({
      relay_id,
      state: Boolean(state),
      mode: mode || 'manual',
      changed_by: changed_by || 'user'
    });

    await relayState.save();

    await SystemLog.create({
      level: 'info',
      source: 'api',
      message: `Relay ${relay_id} changed to ${state ? 'ON' : 'OFF'} (${mode || 'manual'})`
    });

    res.status(201).json({ success: true, data: relayState });
  } catch (error) {
    console.error('Error guardando relay state:', error);
    res.status(500).json({ error: 'Error interno del servidor' });
  }
});

// GET /api/relays/states - Obtener últimos estados de todos los relays
app.get('/api/relays/states', async (req, res) => {
  try {
    const states = await Promise.all([0, 1, 2, 3].map(async (relay_id) => {
      const latest = await RelayState
        .findOne({ relay_id })
        .sort({ timestamp: -1 });
      
      return {
        relay_id,
        state: latest?.state || false,
        mode: latest?.mode || 'manual',
        timestamp: latest?.timestamp || null
      };
    }));

    res.json({ success: true, data: states });
  } catch (error) {
    console.error('Error obteniendo relay states:', error);
    res.status(500).json({ error: 'Error interno del servidor' });
  }
});

// GET /api/rules - Obtener todas las reglas
app.get('/api/rules', async (req, res) => {
  try {
    const relay_id = req.query.relay_id ? parseInt(req.query.relay_id) : undefined;
    const query = relay_id !== undefined ? { relay_id } : {};
    
    const rules = await Rule.find(query).sort({ relay_id: 1, created_at: 1 });
    res.json({ success: true, data: rules, count: rules.length });
  } catch (error) {
    console.error('Error obteniendo rules:', error);
    res.status(500).json({ error: 'Error interno del servidor' });
  }
});

// POST /api/rules - Crear nueva regla
app.post('/api/rules', async (req, res) => {
  try {
    const { relay_id, condition, action, name, enabled } = req.body;

    if (relay_id === undefined || !condition || !action) {
      return res.status(400).json({ error: 'relay_id, condition y action son requeridos' });
    }

    if (relay_id < 0 || relay_id > 3) {
      return res.status(400).json({ error: 'relay_id debe estar entre 0 y 3' });
    }

    const rule = new Rule({
      relay_id,
      condition,
      action,
      name: name || `Rule for relay ${relay_id}`,
      enabled: enabled !== undefined ? enabled : true
    });

    await rule.save();

    await SystemLog.create({
      level: 'info',
      source: 'api',
      message: `New rule created for relay ${relay_id}`,
      metadata: { rule_id: rule._id }
    });

    res.status(201).json({ success: true, data: rule });
  } catch (error) {
    console.error('Error creando rule:', error);
    res.status(500).json({ error: 'Error interno del servidor' });
  }
});

// PUT /api/rules/:id - Actualizar regla existente
app.put('/api/rules/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const updates = req.body;

    const rule = await Rule.findByIdAndUpdate(
      id,
      { ...updates, updated_at: Date.now() },
      { new: true, runValidators: true }
    );

    if (!rule) {
      return res.status(404).json({ error: 'Regla no encontrada' });
    }

    await SystemLog.create({
      level: 'info',
      source: 'api',
      message: `Rule ${id} updated`,
      metadata: { rule_id: id }
    });

    res.json({ success: true, data: rule });
  } catch (error) {
    console.error('Error actualizando rule:', error);
    res.status(500).json({ error: 'Error interno del servidor' });
  }
});

// DELETE /api/rules/:id - Eliminar regla
app.delete('/api/rules/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const rule = await Rule.findByIdAndDelete(id);

    if (!rule) {
      return res.status(404).json({ error: 'Regla no encontrada' });
    }

    await SystemLog.create({
      level: 'info',
      source: 'api',
      message: `Rule ${id} deleted`,
      metadata: { rule_id: id, relay_id: rule.relay_id }
    });

    res.json({ success: true, message: 'Regla eliminada' });
  } catch (error) {
    console.error('Error eliminando rule:', error);
    res.status(500).json({ error: 'Error interno del servidor' });
  }
});

// GET /api/logs - Obtener logs del sistema
app.get('/api/logs', async (req, res) => {
  try {
    const limit = parseInt(req.query.limit) || 100;
    const level = req.query.level;
    const source = req.query.source;

    const query = {};
    if (level) query.level = level;
    if (source) query.source = source;

    const logs = await SystemLog
      .find(query)
      .sort({ timestamp: -1 })
      .limit(limit);

    res.json({ success: true, data: logs, count: logs.length });
  } catch (error) {
    console.error('Error obteniendo logs:', error);
    res.status(500).json({ error: 'Error interno del servidor' });
  }
});

// POST /api/logs - Crear log desde ESP32
app.post('/api/logs', async (req, res) => {
  try {
    const { level, message, metadata } = req.body;

    const log = new SystemLog({
      level: level || 'info',
      source: 'esp32',
      message,
      metadata: metadata || {}
    });

    await log.save();
    res.status(201).json({ success: true, data: log });
  } catch (error) {
    console.error('Error creando log:', error);
    res.status(500).json({ error: 'Error interno del servidor' });
  }
});

// ====== Error Handler ======
app.use((err, req, res, next) => {
  console.error('Error no manejado:', err);
  res.status(500).json({ error: 'Error interno del servidor' });
});

// ====== Start Server ======
app.listen(PORT, () => {
  console.log(`✓ Greenhouse API corriendo en puerto ${PORT}`);
  console.log(`✓ Modo: ${process.env.NODE_ENV || 'development'}`);
  console.log(`✓ Health check: http://localhost:${PORT}/health`);
});

// Graceful shutdown
process.on('SIGTERM', () => {
  console.log('SIGTERM recibido, cerrando servidor...');
  mongoose.connection.close();
  process.exit(0);
});
