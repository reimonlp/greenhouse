const mongoose = require('mongoose');

// ====== MongoDB Connection ======
async function connectDatabase() {
  try {
    await mongoose.connect(process.env.MONGODB_URI, {
      useNewUrlParser: true,
      useUnifiedTopology: true
    });
    // Silent MongoDB connection success
  } catch (err) {
    console.error('✗ Error conectando MongoDB:', err);
    process.exit(1);
  }
}

module.exports = {
  connectDatabase
};