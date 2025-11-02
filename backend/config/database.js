const mongoose = require('mongoose');
const RelayState = require('../models/RelayState');

// ====== MongoDB Connection ======
async function connectDatabase() {
  try {
    await mongoose.connect(process.env.MONGODB_URI, {
    });
    console.log('✓ Conectado a MongoDB');
    
    // Initialize relay states if they don't exist
    await initializeRelayStates();
  } catch (err) {
    console.error('✗ Error conectando MongoDB:', err);
    process.exit(1);
  }
}

/**
 * Initialize relay states in database if they don't exist
 * Ensures relay:states query always returns valid data
 */
async function initializeRelayStates() {
  try {
    const RELAY_NAMES = {
      0: 'Lights',
      1: 'Fan',
      2: 'Pump',
      3: 'Heater'
    };

    for (const relayId of [0, 1, 2, 3]) {
      const exists = await RelayState.findOne({ relay_id: relayId });
      if (!exists) {
        await RelayState.create({
          relay_id: relayId,
          state: false,
          mode: 'manual',
          changed_by: 'system',
          timestamp: new Date()
        });
        console.log(`✓ Relay ${relayId} (${RELAY_NAMES[relayId]}) initialized`);
      }
    }
  } catch (err) {
    console.error('✗ Error initializing relay states:', err);
  }
}

module.exports = {
  connectDatabase
};