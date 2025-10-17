const { checkSocketRateLimit } = require('../middleware/rateLimiter');

// Models
const SensorReading = require('../models/SensorReading');
const RelayState = require('../models/RelayState');
const SystemLog = require('../models/SystemLog');

// Variables that will be set from main server
let io = null;
let ESP32_AUTH_TOKEN = '';
let evaluateSensorRules = async () => {};

function setupSocketHandlers(ioInstance, esp32Token, evaluateSensorRulesFn) {
  io = ioInstance;
  ESP32_AUTH_TOKEN = esp32Token;
  evaluateSensorRules = evaluateSensorRulesFn;

  // Socket.IO connection handler
  io.on('connection', (socket) => {
    // Silent connection - no log noise

    // Enviar datos iniciales al conectar
    socket.emit('connected', {
      message: 'Conectado al servidor Greenhouse',
      timestamp: new Date()
    });

    // ====== ESP32 Device Events ======

    // Device registration with authentication
    socket.on('device:register', async (data) => {
      // Silent attempt - only log failures and successes

      // Validate authentication token
      if (!data.auth_token || data.auth_token !== ESP32_AUTH_TOKEN) {
        const realIP = socket.handshake.headers['x-forwarded-for']?.split(',')[0] || socket.handshake.address;
        console.log('🚨 [SECURITY] Authentication failed for device:', data.device_id, '| IP:', realIP);
        socket.emit('device:auth_failed', {
          error: 'Invalid authentication token',
          device_id: data.device_id
        });
        socket.disconnect(true);
        return;
      }

      // Silent authentication success - no log noise for routine connections
      // Log successful authentication (important event)
      /*
      const realIP = socket.handshake.headers['x-forwarded-for']?.split(',')[0] || socket.handshake.address;
      console.log('✅ [AUTH] Device authenticated:', data.device_id, '| Firmware:', data.firmware_version, '| IP:', realIP);
      */
      socket.deviceId = data.device_id;
      socket.deviceType = data.device_type;
      socket.authenticated = true;

      // Join device room for targeted messages
      socket.join('esp32_devices');

      // Send success confirmation
      socket.emit('device:auth_success', {
        device_id: data.device_id,
        message: 'Authentication successful'
      });

      // Silent connection - no logging for routine connections
      // Log connection without exposing sensitive data
      /*
      const sanitizedData = {
        device_id: data.device_id,
        device_type: data.device_type,
        firmware_version: data.firmware_version,
        ip: socket.handshake.address
        // auth_token deliberately excluded for security
      };

      await SystemLog.create({
        level: 'info',
        message: `ESP32 device ${data.device_id} connected via WebSocket`,
        metadata: sanitizedData
      });
      */
    });

    // Sensor data from ESP32
    socket.on('sensor:data', async (data) => {
      // Check rate limit
      if (!checkSocketRateLimit(socket, 'sensor:data')) {
        socket.emit('error', {
          message: 'Rate limit exceeded. Please slow down.',
          code: 'RATE_LIMIT_EXCEEDED'
        });
        return;
      }

      // Check authentication
      if (!socket.authenticated) {
        console.log('🚨 [SECURITY] Unauthorized sensor:data attempt from:', socket.id);
        return;
      }

      // Silent processing - no log spam for normal sensor data

      try {
        const sensorReading = await SensorReading.create({
          device_id: data.device_id || 'ESP32_GREENHOUSE_01',
          temperature: data.temperature,
          humidity: data.humidity,
          soil_moisture: data.soil_moisture,
          temp_errors: data.temp_errors || 0,
          humidity_errors: data.humidity_errors || 0
        });

        // Broadcast to all connected clients (dashboard)
        io.emit('sensor:new', sensorReading);

        // Evaluate sensor-based rules (will log if thresholds exceeded)
        await evaluateSensorRules(sensorReading);
      } catch (error) {
        console.error('❌ [ERROR] Failed to save sensor data:', error.message);
      }
    });

    // Relay state update from ESP32
    socket.on('relay:state', async (data) => {
      // Check rate limit
      if (!checkSocketRateLimit(socket, 'relay:state')) {
        socket.emit('error', {
          message: 'Rate limit exceeded. Please slow down.',
          code: 'RATE_LIMIT_EXCEEDED'
        });
        return;
      }

      // Check authentication
      if (!socket.authenticated) {
        console.log('🚨 [SECURITY] Unauthorized relay:state attempt from:', socket.id);
        return;
      }

      // Log relay changes (important event)
      const relayNames = ['Luces', 'Ventilador', 'Bomba', 'Calefactor'];
      const relayName = relayNames[data.relay_id] || `Relay ${data.relay_id}`;
      console.log(`🔌 [RELAY] ${relayName} → ${data.state ? 'ON' : 'OFF'} | Mode: ${data.mode || 'manual'} | By: ${data.changed_by || 'esp32'}`);

      try {
        const relayState = await RelayState.findOneAndUpdate(
          { relay_id: data.relay_id },
          {
            state: data.state,
            mode: data.mode || 'manual',
            changed_by: data.changed_by || 'esp32',
            timestamp: new Date()
          },
          { upsert: true, new: true }
        );

        // Broadcast to all connected clients (dashboard)
        io.emit('relay:changed', relayState);

        // Log to SystemLog for audit trail
        await SystemLog.create({
          level: 'info',
          message: `Relay ${data.relay_id} (${relayName}) changed to ${data.state ? 'ON' : 'OFF'}`,
          metadata: {
            relay_id: data.relay_id,
            relay_name: relayName,
            state: data.state,
            mode: data.mode || 'manual',
            changed_by: data.changed_by || 'esp32'
          }
        });
      } catch (error) {
        console.error('❌ [ERROR] Failed to update relay state in database:', error.message);
        console.error('   Relay data:', JSON.stringify({ relay_id: data.relay_id, state: data.state, mode: data.mode }));
      }
    });

    // Log from ESP32
    socket.on('log', async (data) => {
      // Check rate limit
      if (!checkSocketRateLimit(socket, 'log')) {
        socket.emit('error', {
          message: 'Rate limit exceeded. Please slow down.',
          code: 'RATE_LIMIT_EXCEEDED'
        });
        return;
      }

      // Only log warnings and errors from ESP32
      if (data.level === 'warn' || data.level === 'error') {
        console.log(`⚠️ [ESP32-${data.level.toUpperCase()}]`, data.message);
      }

      try {
        await SystemLog.create({
          level: data.level || 'info',
          message: data.message,
          metadata: { device_id: data.device_id }
        });
      } catch (error) {
        console.error('❌ [ERROR] Failed to save ESP32 log entry to database:', error.message);
        console.error('   Log data:', JSON.stringify({ level: data.level, message: data.message?.substring(0, 100) }));
      }
    });

    // Ping/Pong for keepalive
    socket.on('ping', (data) => {
      // Silent ping/pong - no log spam
      socket.emit('pong', { timestamp: new Date() });
    });

    // ESP32 metrics reporting
    socket.on('metrics', (data) => {
      // Store metrics on socket for access via health endpoint
      if (socket.authenticated && socket.deviceType === 'esp32') {
        socket.metrics = data;
        // Log significant metrics updates
        if (data.authFailures > 0 || data.reconnections > 5) {
          console.log(`📊 [METRICS] ${socket.deviceId} - Reconnections: ${data.reconnections}, Auth Failures: ${data.authFailures}, Uptime: ${data.uptimeSeconds}s`);
        }
      }
    });

    socket.on('disconnect', () => {
      // Clean up rate limit data for this socket
      // Note: rate limit cleanup is handled in rateLimiter.js

      // Silent disconnect for ESP32 devices - no log noise for routine disconnections
      // Only log ESP32 device disconnections (important)
      /*
      if (socket.deviceId) {
        console.log('⚠️ [DISCONNECT] ESP32 device disconnected:', socket.deviceId);
      }
      */
      // Silent disconnect for dashboard clients
    });
  });
}

module.exports = {
  setupSocketHandlers
};