// Script para probar eventos WebSocket con socket.io-client
// Ejecuta: node scripts/test_socketio.js

const { io } = require('socket.io-client');

const SERVER_URL = process.env.TEST_WS_URL || 'https://reimon.dev'; // Cambia por tu dominio
const SOCKET_PATH = '/greenhouse/socket.io/';

const socket = io(SERVER_URL, {
  path: SOCKET_PATH,
  transports: ['websocket'],
  reconnection: false,
  rejectUnauthorized: false // Si usas certificados autofirmados
});

socket.on('connect', () => {
  console.log('✅ Conectado al backend Socket.IO');
  // Solicita estados de relés
  socket.emit('relay:states');
});

socket.on('relay:states', (data) => {
  console.log('📡 Evento relay:states recibido:', data);
  socket.disconnect();
});

socket.on('connect_error', (err) => {
  console.error('❌ Error de conexión:', err.message);
});

socket.on('disconnect', (reason) => {
  console.log('🔌 Desconectado:', reason);
});
