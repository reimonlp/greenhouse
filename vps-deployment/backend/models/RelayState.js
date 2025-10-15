const mongoose = require('mongoose');

const relayStateSchema = new mongoose.Schema({
  relay_id: {
    type: Number,
    required: true,
    min: 0,
    max: 3
  },
  state: {
    type: Boolean,
    required: true
  },
  mode: {
    type: String,
    enum: ['manual', 'auto'],
    default: 'manual'
  },
  changed_by: {
    type: String,
    enum: ['user', 'rule', 'system'],
    default: 'user'
  },
  timestamp: {
    type: Date,
    default: Date.now,
    index: true
  }
}, {
  timestamps: true
});

relayStateSchema.index({ relay_id: 1, timestamp: -1 });

module.exports = mongoose.model('RelayState', relayStateSchema);
