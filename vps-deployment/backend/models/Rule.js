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
  condition: {
    sensor: {
      type: String,
      enum: ['temperature', 'humidity', 'soil_moisture'],
      required: true
    },
    operator: {
      type: String,
      enum: ['>', '<', '>=', '<=', '=='],
      required: true
    },
    threshold: {
      type: Number,
      required: true
    }
  },
  action: {
    type: String,
    enum: ['turn_on', 'turn_off'],
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

ruleSchema.pre('save', function(next) {
  this.updated_at = Date.now();
  next();
});

module.exports = mongoose.model('Rule', ruleSchema);
