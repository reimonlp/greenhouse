FROM node:20-alpine AS frontend-builder

WORKDIR /app/frontend

# Copy frontend package files
#COPY frontend/package*.json ./

# Install dependencies
#RUN npm ci --prefer-offline --no-audit

# Copy frontend source
#COPY frontend/ ./

# Build frontend
#RUN npm run build

FROM node:20-alpine

# Utilidades para debug y healthcheck
RUN apk add --no-cache curl bash jq git \
    && rm -rf /var/cache/apk/*

# Directorio de trabajo para backend
WORKDIR /app/backend

# Los archivos del backend y el build del frontend se montan como volúmenes desde el host
# Ejemplo en docker-compose:
#   - ./backend:/app/backend
#   - ./frontend/dist:/app/frontend/dist

# Healthcheck para Express
RUN echo '#!/bin/sh' > /healthcheck.sh && \
    echo 'curl -sf http://localhost:3000/health > /dev/null 2>&1 || exit 1' >> /healthcheck.sh && \
    chmod +x /healthcheck.sh

EXPOSE 3000
HEALTHCHECK --interval=30s --timeout=10s --start-period=15s --retries=3 \
  CMD /healthcheck.sh

LABEL maintainer="Reimon <info@reimon.dev>"
LABEL description="Greenhouse Monitoring System - Express serves Frontend + API (dev mode)"
LABEL version="dev"
STOPSIGNAL SIGTERM

# El backend debe tener node_modules instalados en el host (o puedes instalar manualmente dentro del contenedor)
# Para desarrollo, solo ejecuta el backend:
CMD ["npm", "start"]