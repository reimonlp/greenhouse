# Multi-stage Dockerfile for Greenhouse System
# Stage 1: Build Frontend
FROM node:20-alpine AS frontend-builder

WORKDIR /app/frontend

# Copy frontend package files
COPY greenhouse-dashboard/package*.json ./

# Install dependencies
RUN npm ci --only=production

# Copy frontend source
COPY greenhouse-dashboard/ ./

# Build frontend
RUN npm run build

# Stage 2: Setup Backend and Nginx
FROM node:20-alpine AS backend

# Install nginx and curl
RUN apk add --no-cache nginx curl

# Create nginx user and directories
RUN adduser -D -g 'www' www && \
    mkdir -p /www /run/nginx /var/log/nginx && \
    chown -R www:www /www /run/nginx /var/log/nginx

# Setup backend
WORKDIR /app/backend

# Copy backend package files
COPY backend-websocket-update/package*.json ./

# Install backend dependencies
RUN npm ci --only=production

# Copy backend source
COPY backend-websocket-update/ ./

# Copy built frontend from previous stage
COPY --from=frontend-builder /app/frontend/dist /www

# Copy nginx configuration
COPY nginx.conf /etc/nginx/nginx.conf

# Create .env file with environment variables
RUN echo "PORT=8080" > .env && \
    echo "MONGODB_URI=mongodb://host.docker.internal:27017/greenhouse" >> .env && \
    echo "ALLOWED_ORIGINS=http://localhost:3000,https://greenhouse.reimon.dev" >> .env && \
    echo "ESP32_AUTH_TOKEN=esp32_gh_prod_tk_9f8e7d6c5b4a3210fedcba9876543210abcdef1234567890" >> .env

# Expose ports
EXPOSE 3000 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:3000/health || exit 1

# Copy start script
COPY start.sh /start.sh
RUN chmod +x /start.sh

# Start both nginx and backend
CMD ["/start.sh"]