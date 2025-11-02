/**
 * Frontend Route Handler
 * Serves React app and static assets
 */

const express = require('express');
const path = require('path');

function setupFrontendRoutes(app) {
  const frontendPath = path.join(__dirname, '../frontend/dist');

  // Serve static files (CSS, JS, images, etc.)
  // The frontend is built with base: '/greenhouse/', so we need to serve it correctly
  app.use('/greenhouse/assets', express.static(path.join(frontendPath, 'assets')));
  app.use('/greenhouse/favicon.svg', express.static(path.join(frontendPath, 'favicon.svg')));

  // Also serve without /greenhouse prefix for direct backend access (local dev)
  app.use('/assets', express.static(path.join(frontendPath, 'assets')));
  app.use('/favicon.svg', express.static(path.join(frontendPath, 'favicon.svg')));

  // Serve the main React app for all non-API routes (React Router)
  app.get('*', (req, res, next) => {
    // Skip health check and WebSocket routes
    if (req.path.startsWith('/socket.io/') || req.path === '/health' || req.path.startsWith('/api/')) {
      return next();
    }
    
    // Serve index.html for all other routes
    res.sendFile(path.join(frontendPath, 'index.html'));
  });
}

module.exports = { setupFrontendRoutes };
