/**
 * Rule Engine for Greenhouse Automation
 * Handles sensor-based and time-based rule evaluation
 * 
 * Two types of rules:
 * 1. Sensor-based: Triggered when sensor values exceed thresholds
 * 2. Time-based: Triggered at scheduled times on specific days
 */

const Rule = require('../models/Rule');
const RelayState = require('../models/RelayState');
const SystemLog = require('../models/SystemLog');

// Map relay IDs to names (must match frontend)
const RELAY_NAMES = {
  0: 'Lights',
  1: 'Fan',
  2: 'Pump',
  3: 'Heater'
};

/**
 * Evaluate sensor-based rules
 * Called when new sensor data arrives from ESP32
 * 
 * @param {Object} sensorReading - Latest sensor data {temperature, humidity, soil_moisture}
 * @param {Object} io - Socket.IO instance for broadcasting relay:command
 * @returns {Promise<void>}
 */
async function evaluateSensorRules(sensorReading, io) {
  try {
    // Find all enabled sensor-based rules
    const sensorRules = await Rule.find({
      enabled: true,
      rule_type: 'sensor'
    }).lean();

    if (sensorRules.length === 0) {
      return;
    }

    for (const rule of sensorRules) {
      const { condition, action, relay_id } = rule;
      
      if (!condition || !condition.sensor || !condition.operator || condition.threshold === undefined) {
        console.warn(`⚠️  [WARN] Invalid sensor rule condition: ${rule._id}`);
        continue;
      }

      // Get sensor value
      const sensorValue = sensorReading[condition.sensor];
      if (sensorValue === undefined) {
        continue;
      }

      // Evaluate condition
      const conditionMet = evaluateCondition(sensorValue, condition.operator, condition.threshold);
      
      if (conditionMet) {
        const actionState = action === 'turn_on' || action === 'on';
        console.log(`✅ [RULE] Sensor rule triggered: ${rule.name || `Rule ${rule._id}`}`);
        console.log(`   Sensor: ${condition.sensor}, Value: ${sensorValue}, Operator: ${condition.operator}, Threshold: ${condition.threshold}`);
        console.log(`   Action: ${action} (Relay ${relay_id} - ${RELAY_NAMES[relay_id] || 'Unknown'})`);

        // Execute the action
        await executeRelayAction(relay_id, actionState, 'rule', rule._id, io);
      }
    }
  } catch (error) {
    console.error('❌ [ERROR] Failed to evaluate sensor rules:', error.message);
  }
}

/**
 * Evaluate time-based rules
 * Called every minute to check if scheduled rules should trigger
 * 
 * @param {Object} io - Socket.IO instance for broadcasting relay:command
 * @returns {Promise<void>}
 */
async function evaluateTimeRules(io) {
  try {
    // Find all enabled time-based rules
    const timeRules = await Rule.find({
      enabled: true,
      rule_type: 'time'
    }).lean();

    if (timeRules.length === 0) {
      return;
    }

    const now = new Date();
    const currentHour = String(now.getHours()).padStart(2, '0');
    const currentMinute = String(now.getMinutes()).padStart(2, '0');
    const currentTime = `${currentHour}:${currentMinute}`;
    const dayOfWeek = now.getDay(); // 0=Sunday, 1=Monday, ..., 6=Saturday

    for (const rule of timeRules) {
      const { schedule, action, relay_id } = rule;

      if (!schedule || !schedule.time) {
        console.warn(`⚠️  [WARN] Invalid time rule schedule: ${rule._id}`);
        continue;
      }

      // Check if time matches
      if (schedule.time !== currentTime) {
        continue;
      }

      // Check if day matches (if days are specified)
      const ruleDays = schedule.days && schedule.days.length > 0 ? schedule.days : [0, 1, 2, 3, 4, 5, 6];
      if (!ruleDays.includes(dayOfWeek)) {
        continue;
      }

      // Time and day match - execute action
      const actionState = action === 'turn_on' || action === 'on';
      console.log(`✅ [RULE] Time rule triggered: ${rule.name || `Rule ${rule._id}`}`);
      console.log(`   Scheduled time: ${schedule.time}, Day: ${dayOfWeek}, Action: ${action}`);
      console.log(`   Action: ${action} (Relay ${relay_id} - ${RELAY_NAMES[relay_id] || 'Unknown'})`);

      // Execute the action
      await executeRelayAction(relay_id, actionState, 'rule', rule._id, io);
    }
  } catch (error) {
    console.error('❌ [ERROR] Failed to evaluate time rules:', error.message);
  }
}

/**
 * Helper: Evaluate a condition against a value
 * 
 * @param {number} value - The sensor value
 * @param {string} operator - One of: '>', '<', '>=', '<=', '=='
 * @param {number} threshold - The threshold to compare against
 * @returns {boolean} - Whether the condition is met
 */
function evaluateCondition(value, operator, threshold) {
  switch (operator) {
    case '>':
      return value > threshold;
    case '<':
      return value < threshold;
    case '>=':
      return value >= threshold;
    case '<=':
      return value <= threshold;
    case '==':
      return Math.abs(value - threshold) < 0.01; // Float comparison
    default:
      return false;
  }
}

/**
 * Helper: Execute relay action (turn on/off)
 * This saves the state to the database, logs the action, and broadcasts to ESP32
 * 
 * @param {number} relayId - 0=Lights, 1=Fan, 2=Pump, 3=Heater
 * @param {boolean} state - true=on, false=off
 * @param {string} mode - 'manual' | 'auto' | 'rule' | 'system'
 * @param {string} changedBy - Rule ID or user identifier
 * @param {Object} io - Socket.IO instance for broadcasting relay:command
 * @returns {Promise<Object>} - Updated relay state
 */
async function executeRelayAction(relayId, state, mode = 'rule', changedBy = 'system', io = null) {
  try {
    // Validate relay ID
    if (relayId < 0 || relayId > 3) {
      console.warn(`⚠️  [WARN] Invalid relay ID: ${relayId}`);
      return null;
    }

    // Save relay state to database
    const relayState = await RelayState.create({
      relay_id: relayId,
      state: state,
      mode: mode,
      changed_by: String(changedBy),
      timestamp: new Date()
    });

    // Log the action
    await SystemLog.create({
      event_type: 'relay_change',
      relay_id: relayId,
      message: `Relay ${relayId} (${RELAY_NAMES[relayId] || 'Unknown'}) turned ${state ? 'on' : 'off'} via ${mode} mode`,
      data: {
        relay_id: relayId,
        state: state,
        mode: mode,
        changed_by: changedBy
      }
    });

    // Broadcast relay:command to ESP32 devices (same as dashboard command)
    if (io) {
      io.to('esp32_devices').emit('relay:command', {
        relay_id: relayId,
        state: state,
        mode: mode,
        changed_by: changedBy,
        timestamp: new Date()
      });
      console.log(`📡 [BROADCAST] relay:command sent to ESP32 devices`);
    }

    // Broadcast relay state change to all connected clients (dashboard)
    if (io) {
      io.emit('relay:changed', {
        relay_id: relayId,
        state: state,
        mode: mode,
        changed_by: changedBy,
        timestamp: new Date()
      });
    }

    return relayState;
  } catch (error) {
    console.error(`❌ [ERROR] Failed to execute relay action: ${error.message}`);
    return null;
  }
}

module.exports = {
  evaluateSensorRules,
  evaluateTimeRules
};
