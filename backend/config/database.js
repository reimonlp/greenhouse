const mongoose = require('mongoose');

// ====== MongoDB Connection ======
async function connectDatabase() {
  try {
    await mongoose.connect(process.env.MONGODB_URI, {
    });
    console.log('✓ Conectado a MongoDB');
  } catch (err) {
    console.error('✗ Error conectando MongoDB:', err);
    process.exit(1);
  }
}

module.exports = {
  connectDatabase
};