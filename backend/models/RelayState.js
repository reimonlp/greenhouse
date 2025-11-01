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

// Composite index for efficient relay history queries
relayStateSchema.index({ relay_id: 1, timestamp: -1 });

// TTL index: auto-delete relay state history after 30 days (2592000 seconds)
relayStateSchema.index({ createdAt: 1 }, { expireAfterSeconds: 2592000 });

module.exports = mongoose.model('RelayState', relayStateSchema);
