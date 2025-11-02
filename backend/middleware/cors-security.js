/**
 * CORS and Security Middleware
 * Configures helmet, CORS, and morgan logging
 */

const helmet = require('helmet');
const cors = require('cors');
const morgan = require('morgan');

function setupCorsAndSecurity(app) {
  // Security headers
  app.use(helmet());

  // CORS configuration
  app.use(cors({
    origin: process.env.ALLOWED_ORIGINS?.split(',') || '*'
  }));

  // Body parser
  const express = require('express');
  app.use(express.json());

  // Custom morgan format for real IP addresses behind proxy
  morgan.token('real-ip', (req) => {
    return req.ip || req.connection.remoteAddress;
  });
  app.use(morgan(':real-ip - :method :url :status :res[content-length] - :response-time ms'));

  // Trust proxy (needed behind Nginx)
  app.set('trust proxy', true);
}

module.exports = { setupCorsAndSecurity };
