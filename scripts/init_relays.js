// Script para inicializar los 4 relés en MongoDB
// Ejecutar: node scripts/init_relays.js

require('dotenv').config();
const mongoose = require('mongoose');
const RelayState = require('../backend/models/RelayState');

const MONGODB_URI = process.env.MONGODB_URI || 'mongodb://localhost:27017/greenhouse';

const initialRelays = [
  { relay_id: 0, state: false, mode: 'manual', changed_by: 'system' }, // Luces
  { relay_id: 1, state: false, mode: 'manual', changed_by: 'system' }, // Ventilador
  { relay_id: 2, state: false, mode: 'manual', changed_by: 'system' }, // Bomba
  { relay_id: 3, state: false, mode: 'manual', changed_by: 'system' }  // Calefactor
];

async function initRelays() {
  await mongoose.connect(MONGODB_URI);
  for (const relay of initialRelays) {
    const exists = await RelayState.findOne({ relay_id: relay.relay_id });
    if (!exists) {
      await RelayState.create({
        ...relay,
        timestamp: new Date()
      });
      console.log(`Relay ${relay.relay_id} inicializado.`);
    } else {
      console.log(`Relay ${relay.relay_id} ya existe.`);
    }
  }
  await mongoose.disconnect();
  console.log('Inicialización completa.');
}

initRelays().catch(err => {
  console.error('Error inicializando relés:', err);
  process.exit(1);
});
