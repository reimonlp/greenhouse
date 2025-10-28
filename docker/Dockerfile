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

# Stage 2: Setup Backend (Express serves frontend)
FROM node:20-alpine

# Install curl and other utilities
RUN apk add --no-cache \
    curl \
    bash \
    jq \
    git \
    && rm -rf /var/cache/apk/*

# Create directories
RUN mkdir -p /app/backend/logs

# Setup backend
WORKDIR /app/backend

# Copy backend package files
COPY backend-websocket-update/package*.json ./

# Install backend dependencies with production optimizations
RUN npm ci --prefer-offline --no-audit --omit=dev

# Copy backend source
COPY backend-websocket-update/ ./

COPY --from=frontend-builder /app/frontend/dist /app/frontend/dist

# Create health check script
RUN echo '#!/bin/sh' > /healthcheck.sh && \
    echo 'curl -sf http://localhost:3000/health > /dev/null 2>&1 || exit 1' >> /healthcheck.sh && \
    chmod +x /healthcheck.sh

# Expose port (Express serves both frontend and API)
EXPOSE 3000

# Health check - verify Express server is responding
HEALTHCHECK --interval=30s --timeout=10s --start-period=15s --retries=3 \
  CMD /healthcheck.sh

# Set labels for debugging
LABEL maintainer="Reimon <info@reimon.dev>"
LABEL description="Greenhouse Monitoring System - Express serves Frontend + API"
LABEL version="1.0"

# Graceful shutdown handling
STOPSIGNAL SIGTERM

# Start Express server (serves both frontend and API)
CMD ["npm", "start"]