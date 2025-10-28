import { io } from 'socket.io-client';
import API_BASE_URL from '../config/api';

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

    // Socket.IO debe conectarse al servidor con el path correcto
    // API_BASE_URL es https://reimon.dev/greenhouse
    // Necesitamos conectar a https://reimon.dev con path /greenhouse/socket.io/
    const serverUrl = API_BASE_URL.replace('/greenhouse', '');
    
    // Silent connection - no log noise
    
    this.socket = io(serverUrl, {
      path: '/greenhouse/socket.io/',
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
    }
  }

  // Emitir evento local (listeners internos)
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
