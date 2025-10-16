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
      console.log('WebSocket ya está conectado');
      return;
    }

    console.log('Conectando a WebSocket:', API_BASE_URL);
    
    this.socket = io(API_BASE_URL, {
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
      console.log('✓ WebSocket conectado:', this.socket.id);
      this.reconnectAttempts = 0;
      this.emit('connection:status', { connected: true });
    });

    this.socket.on('disconnect', (reason) => {
      console.log('✗ WebSocket desconectado:', reason);
      this.emit('connection:status', { connected: false, reason });
    });

    this.socket.on('connect_error', (error) => {
      console.error('Error de conexión WebSocket:', error);
      this.reconnectAttempts++;
      
      if (this.reconnectAttempts >= this.maxReconnectAttempts) {
        console.error('Máximo de intentos de reconexión alcanzado');
        this.emit('connection:failed', { error });
      }
    });

    this.socket.on('connected', (data) => {
      console.log('Mensaje del servidor:', data);
    });

    // Eventos del servidor
    this.socket.on('sensor:new', (data) => {
      console.log('Nueva lectura de sensor:', data);
      this.emit('sensor:new', data);
    });

    this.socket.on('relay:changed', (data) => {
      console.log('Estado de relay cambiado:', data);
      this.emit('relay:changed', data);
    });

    this.socket.on('rule:created', (data) => {
      console.log('Regla creada:', data);
      this.emit('rule:created', data);
    });

    this.socket.on('rule:updated', (data) => {
      console.log('Regla actualizada:', data);
      this.emit('rule:updated', data);
    });

    this.socket.on('rule:deleted', (data) => {
      console.log('Regla eliminada:', data);
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
      console.log('Desconectando WebSocket...');
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
