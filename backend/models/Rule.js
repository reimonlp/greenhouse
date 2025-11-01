const mongoose = require('mongoose');

const ruleSchema = new mongoose.Schema({
  relay_id: {
    type: Number,
    required: true,
    min: 0,
    max: 3
  },
  enabled: {
    type: Boolean,
    default: true
  },
  rule_type: {
    type: String,
    enum: ['sensor', 'time'],
    default: 'sensor',
    required: true
  },
  // Condición para reglas basadas en sensores
  condition: {
    sensor: {
      type: String,
      enum: ['temperature', 'humidity', 'soil_moisture']
    },
    operator: {
      type: String,
      enum: ['>', '<', '>=', '<=', '==']
    },
    threshold: {
      type: Number
    }
  },
  // Condición para reglas basadas en tiempo
  schedule: {
    time: {
      type: String,  // Formato "HH:MM" (ej: "06:30", "18:00")
      validate: {
        validator: function(v) {
          return !v || /^([01]\d|2[0-3]):([0-5]\d)$/.test(v);
        },
        message: 'Time must be in HH:MM format (24h)'
      }
    },
    days: {
      type: [Number],  // 0=Domingo, 1=Lunes, ..., 6=Sábado
      default: [0, 1, 2, 3, 4, 5, 6]  // Todos los días por defecto
    }
  },
  action: {
    type: String,
    enum: ['turn_on', 'turn_off', 'on', 'off'],
    required: true
  },
  name: {
    type: String,
    default: ''
  },
  created_at: {
    type: Date,
    default: Date.now
  },
  updated_at: {
    type: Date,
    default: Date.now
  }
});

ruleSchema.index({ relay_id: 1 });
ruleSchema.index({ rule_type: 1, enabled: 1 });

ruleSchema.pre('save', function(next) {
  this.updated_at = Date.now();
  next();
});

module.exports = mongoose.model('Rule', ruleSchema);
