const express = require('express');
const { MongoClient } = require('mongodb');
const cors = require('cors');

const app = express();
const PORT = process.env.PORT || 3000;
const MONGODB_URI = process.env.MONGODB_URI || 'mongodb://admin:greenhouse123@localhost:27017/invernadero?authSource=admin';

let db;
let logsCollection;
let sensorsCollection;

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Conectar a MongoDB
MongoClient.connect(MONGODB_URI, {
  maxPoolSize: 10,
  serverSelectionTimeoutMS: 5000
})
  .then(client => {
    console.log('✅ Conectado a MongoDB');
    db = client.db('invernadero');
    logsCollection = db.collection('system_logs');
    sensorsCollection = db.collection('sensor_data');
  })
  .catch(err => {
    console.error('❌ Error conectando a MongoDB:', err);
    process.exit(1);
  });

// Health check
app.get('/health', (req, res) => {
  res.json({ status: 'ok', timestamp: Date.now() });
});

// POST /api/logs - Insertar log (desde ESP32)
app.post('/api/logs', async (req, res) => {
  try {
    const doc = {
      timestamp: req.body.timestamp || Math.floor(Date.now() / 1000),
      level: parseInt(req.body.level) || 1,
      source: req.body.source || 'unknown',
      message: req.body.message || '',
      data: req.body.data || null
    };
    
    await logsCollection.insertOne(doc);
    res.json({ success: true, inserted: doc.timestamp });
  } catch (error) {
    console.error('Error insertando log:', error);
    res.status(500).json({ error: error.message });
  }
});

// GET /api/logs?count=N - Obtener logs recientes
app.get('/api/logs', async (req, res) => {
  try {
    const count = parseInt(req.query.count) || 50;
    const limit = Math.min(count, 1000); // max 1000
    
    const docs = await logsCollection
      .find({})
      .sort({ timestamp: -1 })
      .limit(limit)
      .toArray();
    
    res.json({ documents: docs });
  } catch (error) {
    console.error('Error obteniendo logs:', error);
    res.status(500).json({ error: error.message });
  }
});

// POST /api/sensors - Insertar dato de sensores
app.post('/api/sensors', async (req, res) => {
  try {
    const doc = {
      timestamp: req.body.timestamp || Math.floor(Date.now() / 1000),
      temperature: parseFloat(req.body.temperature) || 0,
      humidity: parseFloat(req.body.humidity) || 0,
      soil_moisture_1: parseFloat(req.body.soil_moisture_1) || 0,
      soil_moisture_2: parseFloat(req.body.soil_moisture_2) || 0,
      valid: req.body.valid !== false
    };
    
    await sensorsCollection.insertOne(doc);
    res.json({ success: true });
  } catch (error) {
    console.error('Error insertando sensor:', error);
    res.status(500).json({ error: error.message });
  }
});

// GET /api/sensors?from=X&to=Y - Obtener historial de sensores
app.get('/api/sensors', async (req, res) => {
  try {
    const from = parseInt(req.query.from) || 0;
    const to = parseInt(req.query.to) || Math.floor(Date.now() / 1000);
    
    const docs = await sensorsCollection
      .find({ timestamp: { $gte: from, $lte: to } })
      .sort({ timestamp: 1 })
      .limit(1000)
      .toArray();
    
    res.json({ documents: docs });
  } catch (error) {
    console.error('Error obteniendo sensores:', error);
    res.status(500).json({ error: error.message });
  }
});

app.listen(PORT, '0.0.0.0', () => {
  console.log(`🚀 API corriendo en http://0.0.0.0:${PORT}`);
  console.log(`   POST /api/logs - Insertar log`);
  console.log(`   GET  /api/logs?count=N - Obtener logs`);
  console.log(`   POST /api/sensors - Insertar dato sensor`);
  console.log(`   GET  /api/sensors?from=X&to=Y - Obtener sensores`);
});
