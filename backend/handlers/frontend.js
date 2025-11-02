/**
 * Frontend Route Handler
 * Serves React app and static assets
 */

const express = require('express');
const path = require('path');

function setupFrontendRoutes(app) {
  const frontendPath = path.join(__dirname, '../frontend/dist');

  // Serve static assets from /greenhouse/assets (production via Nginx proxy)
  // AND from /assets (local dev without /greenhouse prefix)
  app.use('/greenhouse/assets', express.static(path.join(frontendPath, 'assets')));
  app.use('/assets', express.static(path.join(frontendPath, 'assets')));

  // Serve favicon from both paths
  app.use('/greenhouse/favicon.svg', express.static(path.join(frontendPath, 'favicon.svg')));
  app.use('/favicon.svg', express.static(path.join(frontendPath, 'favicon.svg')));

  // Serve the main React app (index.html) for all non-API routes
  // This handler must be LAST to catch all remaining routes
  app.get('*', (req, res, next) => {
    // Skip WebSocket and health check routes
    if (req.path.startsWith('/socket.io/') || req.path === '/health') {
      return next();
    }
    
    // Skip API routes (if any are added in the future)
    if (req.path.startsWith('/api/')) {
      return next();
    }
    
    // Serve index.html for all other routes (React Router handles routing)
    res.sendFile(path.join(frontendPath, 'index.html'), (err) => {
      if (err) {
        console.error(`Error serving index.html: ${err.message}`);
        res.status(404).send('Not Found');
      }
    });
  });
}

module.exports = { setupFrontendRoutes };
