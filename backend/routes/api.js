const express = require('express');

// Import models
const SensorReading = require('../models/SensorReading');

// Rule engine functions (will be passed from main server)
let evaluateSensorRules = async () => {};
let io = null;
let ESP32_AUTH_TOKEN = '';

function setupApiRoutes(app, ioInstance, esp32Token, evaluateSensorRulesFn) {
  io = ioInstance;
  ESP32_AUTH_TOKEN = esp32Token;
  evaluateSensorRules = evaluateSensorRulesFn;

  // ====== No REST API routes - All communication via WebSocket ======
  // ESP32 sends sensor data via WebSocket 'sensor:data' event
  // Dashboard receives all data via WebSocket events
  // See socketHandlers.js for all WebSocket event handlers

  // ====== Error Handler ======
  app.use((err, req, res, next) => {
    console.error('Error no manejado:', err);
    res.status(500).json({ error: 'Error interno del servidor' });
  });
}

module.exports = {
  setupApiRoutes
};