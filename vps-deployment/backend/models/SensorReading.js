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
  timestamp: {
    type: Date,
    default: Date.now,
    index: true
  }
}, {
  timestamps: true
});

// Índice compuesto para queries eficientes
sensorReadingSchema.index({ device_id: 1, timestamp: -1 });

module.exports = mongoose.model('SensorReading', sensorReadingSchema);
