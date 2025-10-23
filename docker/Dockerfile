# Multi-stage Dockerfile for Greenhouse System
# Stage 1: Build Frontend
FROM node:20-alpine AS frontend-builder

WORKDIR /app/frontend

# Copy frontend package files
COPY greenhouse-dashboard/package*.json ./

# Install dependencies
RUN npm ci --prefer-offline --no-audit

# Copy frontend source
COPY greenhouse-dashboard/ ./

# Build frontend
RUN npm run build

# Verify build
RUN ls -la dist/ && echo "✅ Frontend build successful"

# Stage 2: Setup Backend and Nginx
FROM node:20-alpine

# Install nginx, curl and other utilities
RUN apk add --no-cache \
    nginx \
    curl \
    bash \
    jq \
    && rm -rf /var/cache/apk/*

# Create nginx user and directories with proper permissions
RUN adduser -D -g 'www' www && \
    mkdir -p /www /run/nginx /var/log/nginx /app/backend/logs && \
    chown -R www:www /www /run/nginx /var/log/nginx

# Setup backend
WORKDIR /app/backend

# Copy backend package files
COPY backend-websocket-update/package*.json ./

# Install backend dependencies with production optimizations
RUN npm ci --prefer-offline --no-audit --omit=dev

# Copy backend source
COPY backend-websocket-update/ ./

# Copy built frontend from previous stage
COPY --from=frontend-builder /app/frontend/dist /www

# Copy nginx configuration
COPY docker/nginx.conf /etc/nginx/nginx.conf

# Create health check script
RUN echo '#!/bin/sh' > /healthcheck.sh && \
    echo 'curl -sf http://localhost:3000/health > /dev/null 2>&1 || exit 1' >> /healthcheck.sh && \
    chmod +x /healthcheck.sh

# Expose ports
EXPOSE 3000 8080

# Health check - verify both Nginx and Backend are responding
HEALTHCHECK --interval=30s --timeout=10s --start-period=15s --retries=3 \
  CMD /healthcheck.sh

# Copy start script
COPY docker/start.sh /start.sh
RUN chmod +x /start.sh

# Set labels for debugging
LABEL maintainer="Reimon <info@reimon.dev>"
LABEL description="Greenhouse Monitoring System - Frontend + Backend + Nginx"
LABEL version="1.0"

# Switch to non-root user for security
USER www

# Healthcheck as www user won't work, need to keep root or use healthcheck script
USER root

# Graceful shutdown handling
STOPSIGNAL SIGTERM

# Start both nginx and backend
CMD ["/start.sh"]