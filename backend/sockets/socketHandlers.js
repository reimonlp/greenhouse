const { checkSocketRateLimit } = require('../middleware/rateLimiter');

// Models
const SensorReading = require('../models/SensorReading');
const RelayState = require('../models/RelayState');
const Rule = require('../models/Rule');
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

    // Evento: dashboard solicita todos los estados de relés
    socket.on('relay:states', async () => {
      try {
        // Obtener el estado más reciente de cada relé
        const states = await RelayState.aggregate([
          { $sort: { relay_id: 1, timestamp: -1 } },
          {
            $group: {
              _id: '$relay_id',
              latestState: { $first: '$$ROOT' }
            }
          },
          { $replaceRoot: { newRoot: '$latestState' } },
          { $sort: { relay_id: 1 } }
        ]);

        // Agregar nombres a cada relay para consistencia con API REST
        const RELAY_NAMES = {
          0: 'luces',
          1: 'ventilador',
          2: 'bomba',
          3: 'infrarroja'
        };
        
        const statesWithNames = states.map(state => ({
          ...state,
          name: RELAY_NAMES[state.relay_id] || `relay_${state.relay_id}`
        }));

        socket.emit('relay:states', {
          success: true,
          data: statesWithNames
        });
      } catch (error) {
        socket.emit('relay:states', {
          success: false,
          error: error.message
        });
      }
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

      // Initialize default relay states if they don't exist
      try {
        const RELAY_DEFAULTS = {
          0: { name: 'Luces', default_state: false },
          1: { name: 'Ventilador', default_state: false },
          2: { name: 'Bomba', default_state: false },
          3: { name: 'Infrarroja', default_state: false }
        };

        for (const [relay_id, config] of Object.entries(RELAY_DEFAULTS)) {
          const relayId = parseInt(relay_id);
          const existing = await RelayState.findOne({ relay_id: relayId }).sort({ timestamp: -1 });
          
          if (!existing) {
            // Create initial relay state if it doesn't exist
            await RelayState.create({
              device_id: data.device_id,
              relay_id: relayId,
              state: config.default_state,
              mode: 'manual',
              changed_by: 'system',
              timestamp: new Date()
            });
          }
        }
      } catch (err) {
        console.error('Error initializing relay states:', err.message);
      }

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
        // Si llega external_humidity, usar ese valor para reglas y eventos
        const humidityToUse = (typeof data.external_humidity === 'number' && data.external_humidity >= 0)
          ? data.external_humidity
          : data.humidity;

        // Consulta a Open-Meteo solo si el cache está vencido (usar fetch nativo de Node 18+)
        const LAT_LA_PLATA = -34.9214;
        const LON_LA_PLATA = -57.9544;
        let climateCache = io.climateCache || { value: null, error: null, timestamp: 0 };
        
        let ciudadHumidity = climateCache.value;
        let apiError = climateCache.error;
        const now = Date.now();
        
        if (!climateCache.timestamp || (now - climateCache.timestamp > 5 * 60 * 1000)) {
          try {
            const url = `https://api.open-meteo.com/v1/forecast?latitude=${LAT_LA_PLATA}&longitude=${LON_LA_PLATA}&current_weather=true`;
            const response = await fetch(url);
            const jsonData = await response.json();
            ciudadHumidity = jsonData?.current_weather?.relative_humidity ?? null;
            apiError = null;
            climateCache.value = ciudadHumidity;
            climateCache.error = null;
            climateCache.timestamp = now;
            io.climateCache = climateCache;
          } catch (err) {
            apiError = err.message;
            climateCache.error = apiError;
            climateCache.timestamp = now;
            io.climateCache = climateCache;
          }
        }

        // Emitir evento de tormenta si corresponde
        if (humidityToUse >= 95) {
          io.emit('sensor:storm', {
            message: 'Situación de tormenta detectada',
            sensor_humidity: humidityToUse,
            ciudad: 'La Plata',
            ciudad_humidity: ciudadHumidity,
            api_error: apiError
          });
        }

        const sensorReading = await SensorReading.create({
          device_id: data.device_id || 'ESP32_GREENHOUSE_01',
          temperature: data.temperature,
          humidity: humidityToUse,
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

    // ====== Dashboard Real-time Events (WebSocket Modern API) ======

    // Request log list with optional filters
    socket.on('log:list', async (data) => {
      try {
        // Check rate limit
        if (!checkSocketRateLimit(socket, 'log:list')) {
          socket.emit('error', {
            message: 'Rate limit exceeded. Please slow down.',
            code: 'RATE_LIMIT_EXCEEDED'
          });
          return;
        }

        const limit = data?.limit || 50;
        const level = data?.level;
        const source = data?.source;

        // Build query filter
        const query = {};
        if (level) query.level = level;
        if (source) query['metadata.source'] = source;

        const logs = await SystemLog.find(query)
          .sort({ timestamp: -1 })
          .limit(limit)
          .lean();

        socket.emit('log:list', {
          success: true,
          data: logs,
          count: logs.length,
          timestamp: new Date()
        });
      } catch (error) {
        console.error('❌ [ERROR] Failed to fetch logs:', error.message);
        socket.emit('log:list', {
          success: false,
          error: error.message
        });
      }
    });

    // Request all rules with optional filters
    socket.on('rule:list', async (data) => {
      try {
        // Check rate limit
        if (!checkSocketRateLimit(socket, 'rule:list')) {
          socket.emit('error', {
            message: 'Rate limit exceeded. Please slow down.',
            code: 'RATE_LIMIT_EXCEEDED'
          });
          return;
        }

        const rules = await Rule.find()
          .sort({ priority: -1, createdAt: -1 })
          .lean();

        socket.emit('rule:list', {
          success: true,
          data: rules,
          count: rules.length,
          timestamp: new Date()
        });
      } catch (error) {
        console.error('❌ [ERROR] Failed to fetch rules:', error.message);
        socket.emit('rule:list', {
          success: false,
          error: error.message
        });
      }
    });

    // Create new rule via WebSocket
    socket.on('rule:create', async (data) => {
      try {
        // Check rate limit
        if (!checkSocketRateLimit(socket, 'rule:create')) {
          socket.emit('error', {
            message: 'Rate limit exceeded. Please slow down.',
            code: 'RATE_LIMIT_EXCEEDED'
          });
          return;
        }

        const newRule = new Rule(data);
        await newRule.save();

        // Broadcast to all connected clients
        io.emit('rule:created', {
          success: true,
          data: newRule
        });

        // Log action
        await SystemLog.create({
          level: 'info',
          message: `Rule "${newRule.name}" created`,
          metadata: { rule_id: newRule._id, rule_name: newRule.name }
        });

        socket.emit('rule:create', {
          success: true,
          data: newRule,
          message: 'Rule created successfully'
        });
      } catch (error) {
        console.error('❌ [ERROR] Failed to create rule:', error.message);
        socket.emit('rule:create', {
          success: false,
          error: error.message
        });
      }
    });

    // Update existing rule via WebSocket
    socket.on('rule:update', async (data) => {
      try {
        // Check rate limit
        if (!checkSocketRateLimit(socket, 'rule:update')) {
          socket.emit('error', {
            message: 'Rate limit exceeded. Please slow down.',
            code: 'RATE_LIMIT_EXCEEDED'
          });
          return;
        }

        const { ruleId, ruleData } = data;
        const updatedRule = await Rule.findByIdAndUpdate(
          ruleId,
          ruleData,
          { new: true, runValidators: true }
        );

        if (!updatedRule) {
          socket.emit('rule:update', {
            success: false,
            error: 'Rule not found'
          });
          return;
        }

        // Broadcast to all connected clients
        io.emit('rule:updated', {
          success: true,
          data: updatedRule
        });

        // Log action
        await SystemLog.create({
          level: 'info',
          message: `Rule "${updatedRule.name}" updated`,
          metadata: { rule_id: updatedRule._id, rule_name: updatedRule.name }
        });

        socket.emit('rule:update', {
          success: true,
          data: updatedRule,
          message: 'Rule updated successfully'
        });
      } catch (error) {
        console.error('❌ [ERROR] Failed to update rule:', error.message);
        socket.emit('rule:update', {
          success: false,
          error: error.message
        });
      }
    });

    // Delete rule via WebSocket
    socket.on('rule:delete', async (data) => {
      try {
        // Check rate limit
        if (!checkSocketRateLimit(socket, 'rule:delete')) {
          socket.emit('error', {
            message: 'Rate limit exceeded. Please slow down.',
            code: 'RATE_LIMIT_EXCEEDED'
          });
          return;
        }

        const { ruleId } = data;
        const deletedRule = await Rule.findByIdAndDelete(ruleId);

        if (!deletedRule) {
          socket.emit('rule:delete', {
            success: false,
            error: 'Rule not found'
          });
          return;
        }

        // Broadcast to all connected clients
        io.emit('rule:deleted', {
          success: true,
          data: { id: ruleId, name: deletedRule.name }
        });

        // Log action
        await SystemLog.create({
          level: 'info',
          message: `Rule "${deletedRule.name}" deleted`,
          metadata: { rule_id: deletedRule._id, rule_name: deletedRule.name }
        });

        socket.emit('rule:delete', {
          success: true,
          message: 'Rule deleted successfully'
        });
      } catch (error) {
        console.error('❌ [ERROR] Failed to delete rule:', error.message);
        socket.emit('rule:delete', {
          success: false,
          error: error.message
        });
      }
    });

    // Handle relay:command from dashboard to ESP32
    socket.on('relay:command', async (data) => {
      try {
        // Check rate limit
        if (!checkSocketRateLimit(socket, 'relay:command')) {
          socket.emit('error', {
            message: 'Rate limit exceeded. Please slow down.',
            code: 'RATE_LIMIT_EXCEEDED'
          });
          return;
        }

        const { relay_id, state, mode = 'manual', changed_by = 'user' } = data;

        // Save relay state change
        const relayState = await RelayState.findOneAndUpdate(
          { relay_id },
          {
            state,
            mode,
            changed_by,
            timestamp: new Date()
          },
          { upsert: true, new: true }
        );

        // Broadcast state change to all clients
        io.emit('relay:changed', relayState);

        // Send command to ESP32 device
        io.to('esp32_devices').emit('relay:command', {
          relay_id,
          state,
          mode
        });

        // Log action
        const relayNames = ['Luces', 'Ventilador', 'Bomba', 'Calefactor'];
        const relayName = relayNames[relay_id] || `Relay ${relay_id}`;
        console.log(`🔌 [RELAY_CMD] ${relayName} → ${state ? 'ON' : 'OFF'} | Mode: ${mode} | By: ${changed_by}`);

        await SystemLog.create({
          level: 'info',
          message: `Relay ${relay_id} (${relayName}) commanded to ${state ? 'ON' : 'OFF'} via dashboard`,
          metadata: {
            relay_id,
            relay_name: relayName,
            state,
            mode,
            changed_by
          }
        });

        socket.emit('relay:command', {
          success: true,
          data: relayState,
          message: `${relayName} ${state ? 'encendida' : 'apagada'}`
        });
      } catch (error) {
        console.error('❌ [ERROR] Failed to process relay command:', error.message);
        socket.emit('relay:command', {
          success: false,
          error: error.message
        });
      }
    });

    // Request latest sensor reading
    socket.on('sensor:latest', async () => {
      try {
        // Check rate limit
        if (!checkSocketRateLimit(socket, 'sensor:latest')) {
          socket.emit('error', {
            message: 'Rate limit exceeded. Please slow down.',
            code: 'RATE_LIMIT_EXCEEDED'
          });
          return;
        }

        const latestSensor = await SensorReading.findOne()
          .sort({ timestamp: -1 })
          .lean();

        socket.emit('sensor:latest', {
          success: true,
          data: latestSensor,
          timestamp: new Date()
        });
      } catch (error) {
        console.error('❌ [ERROR] Failed to fetch latest sensor:', error.message);
        socket.emit('sensor:latest', {
          success: false,
          error: error.message
        });
      }
    });

    // Request sensor history with date range
    socket.on('sensor:history', async (data) => {
      try {
        // Check rate limit
        if (!checkSocketRateLimit(socket, 'sensor:history')) {
          socket.emit('error', {
            message: 'Rate limit exceeded. Please slow down.',
            code: 'RATE_LIMIT_EXCEEDED'
          });
          return;
        }

        const limit = data?.limit || 100;
        const startDate = data?.startDate ? new Date(data.startDate) : new Date(Date.now() - 24 * 60 * 60 * 1000);
        const endDate = data?.endDate ? new Date(data.endDate) : new Date();

        const readings = await SensorReading.find({
          timestamp: { $gte: startDate, $lte: endDate }
        })
          .sort({ timestamp: -1 })
          .limit(limit)
          .lean();

        socket.emit('sensor:history', {
          success: true,
          data: readings,
          count: readings.length,
          dateRange: { startDate, endDate },
          timestamp: new Date()
        });
      } catch (error) {
        console.error('❌ [ERROR] Failed to fetch sensor history:', error.message);
        socket.emit('sensor:history', {
          success: false,
          error: error.message
        });
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