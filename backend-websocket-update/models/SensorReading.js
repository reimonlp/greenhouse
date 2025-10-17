const mongoose = require('mongoose');

const sensorReadingSchema = new mongoose.Schema({
  device_id: {
    type: String,
    required: true,
    default: 'ESP32_MAIN'
  },
  temperature: {
    type: Number,
    required: true
  },
  humidity: {
    type: Number,
    required: true
  },
  soil_moisture: {
    type: Number,
    default: null
  },
  temp_errors: {
    type: Number,
    default: 0
  },
  humidity_errors: {
    type: Number,
    default: 0
  },
  timestamp: {
    type: Date,
    default: Date.now,
    index: true
  }
}, {
  timestamps: true
});

// Índice compuesto para queries eficientes (device_id + timestamp)
sensorReadingSchema.index({ device_id: 1, timestamp: -1 });

// TTL index: auto-delete documents after 30 days (2592000 seconds)
// MongoDB will automatically remove documents where createdAt is older than 30 days
sensorReadingSchema.index({ createdAt: 1 }, { expireAfterSeconds: 2592000 });

module.exports = mongoose.model('SensorReading', sensorReadingSchema);
