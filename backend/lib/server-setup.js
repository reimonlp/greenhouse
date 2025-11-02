/**
 * Server Setup
 * Initializes HTTP server and Socket.IO configuration
 */

const http = require('http');
const { Server } = require('socket.io');

function createServer(app) {
  return http.createServer(app);
}

function setupSocketIO(server) {
  const io = new Server(server, {
    cors: {
      origin: process.env.ALLOWED_ORIGINS?.split(',') || "*",
      methods: ["GET", "POST"],
      credentials: true
    },
    path: '/socket.io/',
    // In production behind Nginx proxy at /greenhouse/socket.io/
    // The browser sees /greenhouse/socket.io/ but needs to work both locally and on VPS
    transports: ['websocket', 'polling']
  });

  return io;
}

function startServer(server, port) {
  server.listen(port, () => {
    // Silent server start - no log noise
  });
}

function setupGracefulShutdown(server, io) {
  process.on('SIGTERM', () => {
    console.log('SIGTERM received, closing server...');
    io.close();
    require('mongoose').connection.close();
    server.close(() => {
      process.exit(0);
    });
  });
}

module.exports = {
  createServer,
  setupSocketIO,
  startServer,
  setupGracefulShutdown
};
