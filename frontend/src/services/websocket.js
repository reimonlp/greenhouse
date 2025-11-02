import { io } from 'socket.io-client';

class WebSocketService {
  constructor() {
    this.socket = null;
    this.listeners = new Map();
    this.reconnectAttempts = 0;
    this.maxReconnectAttempts = 5;
  }

  connect() {
    if (this.socket?.connected) {
      // WebSocket ya está conectado - silent
      return;
    }

    // Determine server URL based on environment
    let serverUrl;
    let path;
    
    if (import.meta.env.DEV) {
      // Development: connect to local backend
      serverUrl = 'http://localhost:3000';
      path = '/socket.io/';
    } else {
      // Production: connect to domain root (Nginx proxies to backend)
      serverUrl = 'https://reimon.dev';
      path = '/greenhouse/socket.io/';
    }
    
    // Allow override via environment variable
    if (import.meta.env.VITE_WS_URL) {
      serverUrl = import.meta.env.VITE_WS_URL;
    }
    
    // Log the connection URL in development
    if (import.meta.env.DEV) {
      console.log('[WS] Connecting to:', serverUrl, 'with path:', path);
    }
    
    this.socket = io(serverUrl, {
      path: path,
      transports: ['websocket', 'polling'],
      reconnection: true,
      reconnectionDelay: 1000,
      reconnectionDelayMax: 5000,
      reconnectionAttempts: this.maxReconnectAttempts
    });

    this.setupEventHandlers();
  }

  setupEventHandlers() {
    this.socket.on('connect', () => {
      // Silent connection success
      this.reconnectAttempts = 0;
      this.emit('connection:status', { connected: true });
    });

    this.socket.on('disconnect', (reason) => {
      // Silent disconnection
      this.emit('connection:status', { connected: false, reason });
    });

    this.socket.on('connect_error', (error) => {
      console.error('Error de conexión WebSocket:', error);
      this.reconnectAttempts++;
      
      if (this.reconnectAttempts >= this.maxReconnectAttempts) {
        // Silent - max reconnection attempts reached
        this.emit('connection:failed', { error });
      }
    });

    this.socket.on('connected', (data) => {
      // Silent server message
    });

    // Eventos del servidor
    this.socket.on('sensor:new', (data) => {
      // Silent sensor data
      this.emit('sensor:new', data);
    });

    this.socket.on('relay:changed', (data) => {
      // Silent relay change
      this.emit('relay:changed', data);
    });

    this.socket.on('relay:states', (data) => {
      // Relay states response
      this.emit('relay:states', data);
    });

    this.socket.on('log:list', (data) => {
      // Logs response
      this.emit('log:list', data);
    });

    this.socket.on('rule:list', (data) => {
      // Rules response
      this.emit('rule:list', data);
    });

    this.socket.on('sensor:latest', (data) => {
      // Latest sensor response
      this.emit('sensor:latest', data);
    });

    this.socket.on('sensor:history', (data) => {
      // Sensor history response
      this.emit('sensor:history', data);
    });

    this.socket.on('rule:created', (data) => {
      // Silent rule creation
      this.emit('rule:created', data);
    });

    this.socket.on('rule:updated', (data) => {
      // Silent rule update
      this.emit('rule:updated', data);
    });

    this.socket.on('rule:deleted', (data) => {
      // Silent rule deletion
      this.emit('rule:deleted', data);
    });
  }

  // Sistema de eventos personalizado
  on(event, callback) {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, []);
    }
    this.listeners.get(event).push(callback);
    
    // Retornar función para eliminar el listener
    return () => this.off(event, callback);
  }

  off(event, callback) {
    if (!this.listeners.has(event)) return;
    
    const callbacks = this.listeners.get(event);
    const index = callbacks.indexOf(callback);
    if (index > -1) {
      callbacks.splice(index, 1);
    }
  }

  // Emitir evento al servidor WebSocket
  emitToServer(event, data) {
    if (this.socket && this.socket.connected) {
      this.socket.emit(event, data);
    } else {
      console.error('[WebSocketService] Socket is null or not connected, cannot emit event:', event, data);
    }
  }

  // Emitir evento local (listeners internos)
  // Emit is for local listeners only. Use emitToServer for server events.
  emit(event, data) {
    if (!this.listeners.has(event)) return;
    this.listeners.get(event).forEach(callback => {
      try {
        callback(data);
      } catch (error) {
        console.error(`Error en listener de ${event}:`, error);
      }
    });
  }

  // ====== Helper Methods for Common Operations ======

  /**
   * Send relay command to ESP32 via server
   * @param {number} relayId - Relay ID (0-3)
   * @param {boolean} state - Relay state (on/off)
   * @param {string} mode - Control mode ('manual', 'auto', 'rule')
   * @param {string} changedBy - Who initiated the change
   */
  sendRelayCommand(relayId, state, mode = 'manual', changedBy = 'user') {
    this.emitToServer('relay:command', {
      relay_id: relayId,
      state,
      mode,
      changed_by: changedBy
    });
  }

  /**
   * Fetch latest sensor reading
   */
  fetchLatestSensor() {
    this.emitToServer('sensor:latest');
  }

  /**
   * Fetch sensor history with optional date range
   * @param {number} limit - Max records to fetch (default 100)
   * @param {Date} startDate - Start date for history
   * @param {Date} endDate - End date for history
   */
  fetchSensorHistory(limit = 100, startDate = null, endDate = null) {
    this.emitToServer('sensor:history', {
      limit,
      startDate: startDate?.toISOString(),
      endDate: endDate?.toISOString()
    });
  }

  /**
   * Fetch relay states
   */
  fetchRelayStates() {
    this.emitToServer('relay:states');
  }

  /**
   * Fetch all rules
   */
  fetchRules() {
    this.emitToServer('rule:list');
  }

  /**
   * Create a new rule
   * @param {Object} ruleData - Rule configuration
   */
  createRule(ruleData) {
    this.emitToServer('rule:create', ruleData);
  }

  /**
   * Update an existing rule
   * @param {string} ruleId - Rule ID
   * @param {Object} ruleData - Updated rule configuration
   */
  updateRule(ruleId, ruleData) {
    this.emitToServer('rule:update', { ruleId, ruleData });
  }

  /**
   * Delete a rule
   * @param {string} ruleId - Rule ID to delete
   */
  deleteRule(ruleId) {
    this.emitToServer('rule:delete', { ruleId });
  }

  /**
   * Fetch system logs with optional filters
   * @param {number} limit - Max records to fetch (default 50)
   * @param {string} level - Filter by log level (debug, info, warning, error)
   * @param {string} source - Filter by source
   */
  fetchLogs(limit = 50, level = null, source = null) {
    this.emitToServer('log:list', {
      limit,
      level: level || undefined,
      source: source || undefined
    });
  }

  disconnect() {
    if (this.socket) {
      // Silent disconnection
      this.socket.disconnect();
      this.socket = null;
      this.listeners.clear();
    }
  }

  isConnected() {
    return this.socket?.connected || false;
  }
}

// Instancia singleton
const webSocketService = new WebSocketService();

export default webSocketService;
