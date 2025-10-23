# Plan de Despliegue Docker - Greenhouse

## Índice
1. [Visión General](#visión-general)
2. [Fase 1: Preparación Local](#fase-1-preparación-local)
3. [Fase 2: Testing Local](#fase-2-testing-local)
4. [Fase 3: Preparación VPS](#fase-3-preparación-vps)
5. [Fase 4: Despliegue en VPS](#fase-4-despliegue-en-vps)
6. [Fase 5: Nginx Reverse Proxy](#fase-5-nginx-reverse-proxy)
7. [Fase 6: Validación Final](#fase-6-validación-final)
8. [Fase 7: Mantenimiento](#fase-7-mantenimiento)

---

## Visión General

```
┌─────────────────────────────────────────────────────────────────┐
│                    ARQUITECTURA FINAL                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ETAPA 1 (NETBOOK)             ETAPA 2 (VPS)                    │
│  ───────────────────           ────────────────                  │
│                                                                   │
│  Docker Compose                Docker Compose                    │
│  ├─ Frontend (nginx:3000)      ├─ Frontend (nginx:3000)          │
│  ├─ Backend (node:8080)        ├─ Backend (node:8080)            │
│  ├─ MongoDB:27017              └─ MongoDB:27017                  │
│  │                                                                │
│  └─ Test con ESP32                         ↑                    │
│     Simulado/Real                          │                    │
│                              (SSH + push image o build)          │
│                                            │                    │
│                              ┌─────────────┴──────────┐          │
│                              │   VPS Nginx            │          │
│                              │  ─────────────────     │          │
│                              │ :80/:443               │          │
│                              │  reimon.dev/greenhouse/│          │
│                              │       ↓                │          │
│                              │  Proxy →:3000/:8080   │          │
│                              └────────────────────────┘          │
│                                    ↓                             │
│                              🌐 Public Internet                  │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

### Objetivos
✅ Contenedor único que incluye Frontend + Backend + Nginx  
✅ MongoDB separado (puede ser en Docker o en máquina host)  
✅ Testing completo en local antes de VPS  
✅ Despliegue sin downtime  
✅ Proxy reverso transparente con Nginx  
✅ SSL/HTTPS automático con Let's Encrypt  

---

## Fase 1: Preparación Local

### 1.1 Instalar Docker & Docker Compose

**En Ubuntu/Debian:**
```bash
# Instalar Docker
sudo apt update
sudo apt install -y docker.io docker-compose-v2 git curl

# Agregar usuario actual al grupo docker
sudo usermod -aG docker $USER
newgrp docker

# Verificar instalación
docker --version
docker compose version
```

### 1.2 Estructura de Directorios Local

```bash
# Crear estructura de trabajo
mkdir -p ~/greenhouse-docker/{config,data,logs}
cd ~/greenhouse-docker

# Clonar/copiar proyecto si aún no está
# cp -r /home/reimon/greenhouse .

# Crear estructura de config
mkdir -p config/secrets config/env
```

### 1.3 Crear Archivos de Configuración

**`config/env/local.env`** (para desarrollo local):
```bash
# Backend Configuration
PORT=8080
NODE_ENV=development

# MongoDB
MONGODB_URI=mongodb://mongodb:27017/greenhouse

# CORS - Permitir conexiones locales
ALLOWED_ORIGINS=http://localhost:3000,http://localhost:5173,http://127.0.0.1:3000

# Rate Limiting
RATE_LIMIT_WINDOW_MS=900000
RATE_LIMIT_MAX_REQUESTS=10000

# ESP32 Authentication (IMPORTANT: cambiar en producción)
ESP32_AUTH_TOKEN=local_dev_token_minimum_32_characters_please_change_me_now

# LOGGING
LOG_LEVEL=debug
```

**`config/secrets/production.env`** (crear DESPUÉS, no commitear):
```bash
# Este archivo NO debe estar en git
# .gitignore debe contener: config/secrets/
```

### 1.4 Modificar Dockerfile para Local

**`docker/Dockerfile`** - Actualizar (copiar actual primero):
```dockerfile
# Multi-stage Dockerfile for Greenhouse System
# Stage 1: Build Frontend
FROM node:20-alpine AS frontend-builder

WORKDIR /app/frontend
COPY greenhouse-dashboard/package*.json ./
RUN npm ci
COPY greenhouse-dashboard/ ./
RUN npm run build

# Stage 2: Backend + Nginx
FROM node:20-alpine

# Install nginx and curl
RUN apk add --no-cache nginx curl

# Create nginx user
RUN adduser -D -g 'www' www && \
    mkdir -p /www /run/nginx /var/log/nginx && \
    chown -R www:www /www /run/nginx /var/log/nginx

# Setup backend
WORKDIR /app/backend
COPY backend-websocket-update/package*.json ./
RUN npm ci

# Copy backend source
COPY backend-websocket-update/ ./

# Copy built frontend
COPY --from=frontend-builder /app/frontend/dist /www

# Copy nginx config
COPY docker/nginx.conf /etc/nginx/nginx.conf

# Health check script
RUN echo '#!/bin/sh' > /healthcheck.sh && \
    echo 'curl -f http://localhost:3000/health || exit 1' >> /healthcheck.sh && \
    chmod +x /healthcheck.sh

EXPOSE 3000 8080

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
  CMD /healthcheck.sh

COPY docker/start.sh /start.sh
RUN chmod +x /start.sh

CMD ["/start.sh"]
```

### 1.5 Mejorar Docker Compose (local)

**`docker/docker-compose.yml`** - Versión local mejorada:
```yaml
version: '3.9'

services:
  # MongoDB Database
  mongodb:
    image: mongo:7-alpine
    container_name: greenhouse-mongodb
    ports:
      - "27017:27017"
    volumes:
      - mongodb_data:/data/db
    environment:
      - MONGO_INITDB_DATABASE=greenhouse
    healthcheck:
      test: echo 'db.runCommand("ping").ok' | mongosh localhost:27017/test --quiet
      interval: 10s
      timeout: 5s
      retries: 5
    restart: unless-stopped
    networks:
      - greenhouse-network

  # Main Application (Frontend + Backend + Nginx)
  app:
    build:
      context: ..
      dockerfile: docker/Dockerfile
    container_name: greenhouse-app
    depends_on:
      mongodb:
        condition: service_healthy
    ports:
      - "3000:3000"    # Nginx (Frontend)
      - "8080:8080"    # Backend API
    environment:
      - PORT=8080
      - NODE_ENV=development
      - MONGODB_URI=mongodb://mongodb:27017/greenhouse
      - ALLOWED_ORIGINS=http://localhost:3000,http://localhost:5173,http://127.0.0.1:3000
      - RATE_LIMIT_WINDOW_MS=900000
      - RATE_LIMIT_MAX_REQUESTS=10000
      - ESP32_AUTH_TOKEN=local_dev_token_minimum_32_characters_please_change_me_now
    volumes:
      # Opcional: para desarrollo hot-reload
      - ../backend-websocket-update:/app/backend:ro
      - ../greenhouse-dashboard/dist:/www:ro
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:3000/health"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 10s
    restart: unless-stopped
    networks:
      - greenhouse-network
    labels:
      - "com.greenhouse.description=Greenhouse App Container"
      - "com.greenhouse.version=1.0"

volumes:
  mongodb_data:
    driver: local

networks:
  greenhouse-network:
    driver: bridge
```

### 1.6 Archivo .gitignore para Docker

**`.gitignore`** - Agregar estas líneas:
```gitignore
# Docker & Configuración
docker/.env
docker/.env.local
config/secrets/
config/env/production.env
docker-compose.override.yml

# Node
node_modules/
npm-debug.log*
dist/

# Logs
logs/
*.log

# Base de datos
mongodb_data/
data/

# IDE
.vscode/
.idea/
*.swp
*.swo

# OS
.DS_Store
Thumbs.db
```

---

## Fase 2: Testing Local

### 2.1 Build de Imagen Local

```bash
cd ~/greenhouse-docker
docker compose build

# Ver imagen creada
docker images | grep greenhouse

# Output esperado:
# REPOSITORY          TAG       IMAGE ID       CREATED        SIZE
# greenhouse-docker-app  latest    a1b2c3d4e5f6   2 minutes ago  542MB
```

### 2.2 Iniciar Contenedores

```bash
# Iniciar en background
docker compose up -d

# Verificar estado
docker compose ps

# Ver logs en tiempo real
docker compose logs -f

# Output esperado:
# greenhouse-mongodb | ...init replica set...
# greenhouse-app | [OK] WebSocket ready...
# greenhouse-app | [OK] Nginx started...
```

### 2.3 Verificar Servicios

```bash
# 1. Frontend accesible
curl -I http://localhost:3000
# HTTP/1.1 200 OK

# 2. Backend API
curl -s http://localhost:8080/health | jq .
# {
#   "status": "ok",
#   "database": {"status": "connected"}
# }

# 3. MongoDB
docker exec greenhouse-mongodb mongosh --eval "db.version()"
# 7.0.0

# 4. WebSocket (mediante Node test)
curl -s http://localhost:3000 | grep -i socket
```

### 2.4 Scripts de Testing

**`scripts/test-local.sh`**:
```bash
#!/bin/bash

set -e

echo "🧪 Testing Greenhouse Docker Local..."
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

test_endpoint() {
    local endpoint=$1
    local description=$2
    
    echo -n "Testing $description... "
    if curl -s -f "$endpoint" > /dev/null 2>&1; then
        echo -e "${GREEN}✅ PASS${NC}"
        return 0
    else
        echo -e "${RED}❌ FAIL${NC}"
        return 1
    fi
}

# Test Frontend
test_endpoint "http://localhost:3000" "Frontend"

# Test Health Check
test_endpoint "http://localhost:3000/health" "Health Check"

# Test API
test_endpoint "http://localhost:8080/api/sensors" "API - Sensors"
test_endpoint "http://localhost:8080/api/relays/states" "API - Relays"
test_endpoint "http://localhost:8080/api/logs" "API - Logs"

echo ""
echo "✅ All tests passed!"
```

### 2.5 Simular Conexión ESP32

**`scripts/test-esp32-sim.sh`**:
```bash
#!/bin/bash

# Simular envío de datos de sensor desde ESP32

TOKEN="local_dev_token_minimum_32_characters_please_change_me_now"
BACKEND="http://localhost:8080"

echo "📡 Simulando ESP32 enviando sensor data..."

curl -X POST "$BACKEND/api/sensors" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32_GREENHOUSE_01",
    "temperature": 24.5,
    "humidity": 65.3,
    "soil_moisture": 45,
    "temp_errors": 0,
    "humidity_errors": 0
  }' | jq .

echo ""
echo "✅ Sensor data sent!"

# Verificar que se guardó
echo ""
echo "📊 Verificando últimas lecturas..."
curl -s "$BACKEND/api/sensors/latest" | jq .
```

### 2.6 Monitoreo en Vivo

```bash
# Terminal 1: Ver logs
docker compose logs -f app

# Terminal 2: Ver estado de contenedores
watch -n 2 docker compose ps

# Terminal 3: Monitorear recursos
docker stats greenhouse-app

# Usar dashboard (opcional)
lazydocker  # Si está instalado
```

### 2.7 Parar y Limpiar

```bash
# Parar contenedores (sin eliminar)
docker compose stop

# Reanudar
docker compose start

# Eliminar todo (cuidado!)
docker compose down

# Eliminar volúmenes también
docker compose down -v
```

---

## Fase 3: Preparación VPS

### 3.1 Acceder a VPS

```bash
ssh user@reimon.dev

# O si está configurado en ~/.ssh/config
ssh reimon-vps
```

### 3.2 Instalar Docker en VPS

```bash
# Actualizar paquetes
sudo apt update && sudo apt upgrade -y

# Instalar Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# Instalar Docker Compose
sudo curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-$(uname -s)-$(uname -m)" \
  -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose

# Agregar usuario al grupo docker (opcional pero recomendado)
sudo usermod -aG docker $USER
newgrp docker

# Verificar
docker --version
docker compose version
```

### 3.3 Crear Estructura en VPS

```bash
# Crear directorios de aplicación
sudo mkdir -p /opt/greenhouse
sudo chown -R $USER:$USER /opt/greenhouse
cd /opt/greenhouse

# Estructura
mkdir -p {config,data/mongodb,logs,backups}

# Permisos
chmod 755 config data backups
chmod 600 config/*
```

### 3.4 Backup de Nginx Actual

```bash
# En VPS
cd /opt/greenhouse

# Backup config actual
sudo cp /etc/nginx/sites-available/default backups/nginx.default.backup.$(date +%s)
sudo cp /etc/nginx/nginx.conf backups/nginx.conf.backup.$(date +%s)

# Verificar backup
ls -la backups/
```

### 3.5 Crear .env Production

**`/opt/greenhouse/.env.production`** (en VPS):
```bash
# ⚠️ CREAR DIRECTAMENTE EN VPS (NO PUSHEAR)
# ssh user@reimon.dev

cat > /opt/greenhouse/.env.production << 'EOF'
# Backend Configuration
PORT=8080
NODE_ENV=production

# MongoDB
MONGODB_URI=mongodb://mongodb:27017/greenhouse

# CORS - Dominio de producción
ALLOWED_ORIGINS=https://reimon.dev

# Rate Limiting
RATE_LIMIT_WINDOW_MS=900000
RATE_LIMIT_MAX_REQUESTS=5000

# ESP32 Authentication (GENERAR NUEVO!)
# Generar con: openssl rand -hex 32
ESP32_AUTH_TOKEN=GENERADO_CON_OPENSSL_RAND_32_CHARS_MIN

# Logging
LOG_LEVEL=info
EOF

# Proteger archivo
chmod 600 /opt/greenhouse/.env.production

# Generar token seguro
openssl rand -hex 32
# Copiar salida en ESP32_AUTH_TOKEN

# Verificar
cat /opt/greenhouse/.env.production
```

**Importante**: Guardar el `ESP32_AUTH_TOKEN` generado para luego actualizarlo en `secrets.h` del ESP32.

---

## Fase 4: Despliegue en VPS

### 4.1 Opción A: Build en VPS (Recomendado para pequeño VPS)

```bash
# En VPS
cd /opt/greenhouse

# Clonar repositorio (si está en GitHub)
git clone https://github.com/tu-usuario/greenhouse.git .

# O copiar archivos
scp -r /home/reimon/greenhouse/* user@reimon.dev:/opt/greenhouse/

# Crear docker-compose.yml production
# (ver sección 4.3)

# Build imagen
docker compose -f docker-compose.prod.yml build

# Iniciar
docker compose -f docker-compose.prod.yml up -d
```

### 4.2 Opción B: Push a Registry y Pull en VPS

```bash
# En netbook (local)
cd ~/greenhouse-docker

# Login a Docker Hub
docker login

# Taggear imagen
docker tag greenhouse-docker-app:latest tu-usuario/greenhouse:latest

# Push
docker push tu-usuario/greenhouse:latest

# En VPS
docker pull tu-usuario/greenhouse:latest
docker compose -f docker-compose.prod.yml up -d
```

### 4.3 Docker Compose Producción

**`/opt/greenhouse/docker-compose.prod.yml`** (en VPS):
```yaml
version: '3.9'

services:
  mongodb:
    image: mongo:7-alpine
    container_name: greenhouse-mongodb
    # NO exponer puerto (solo acceso interno)
    volumes:
      - ./data/mongodb:/data/db
    environment:
      - MONGO_INITDB_DATABASE=greenhouse
    healthcheck:
      test: echo 'db.runCommand("ping").ok' | mongosh localhost:27017/test --quiet
      interval: 10s
      timeout: 5s
      retries: 5
    restart: always
    networks:
      - greenhouse-network

  app:
    image: tu-usuario/greenhouse:latest
    # O: build: . (si está en VPS)
    container_name: greenhouse-app
    depends_on:
      mongodb:
        condition: service_healthy
    ports:
      - "127.0.0.1:3000:3000"  # Solo localhost (Nginx hará proxy)
      - "127.0.0.1:8080:8080"  # Solo localhost
    environment:
      - PORT=8080
      - NODE_ENV=production
      - MONGODB_URI=mongodb://mongodb:27017/greenhouse
      - ALLOWED_ORIGINS=https://reimon.dev
      - RATE_LIMIT_WINDOW_MS=900000
      - RATE_LIMIT_MAX_REQUESTS=5000
      - ESP32_AUTH_TOKEN=${ESP32_AUTH_TOKEN}
    volumes:
      - ./logs:/app/backend/logs
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:3000/health"]
      interval: 30s
      timeout: 10s
      retries: 3
    restart: always
    networks:
      - greenhouse-network
    labels:
      - "com.greenhouse.environment=production"

volumes:
  # MongoDB data persiste en disco
  mongodb_data:

networks:
  greenhouse-network:
    driver: bridge
```

### 4.4 Verificar Despliegue en VPS

```bash
# En VPS
cd /opt/greenhouse

# Ver estado
docker compose -f docker-compose.prod.yml ps

# Ver logs
docker compose -f docker-compose.prod.yml logs -f app

# Verificar conectividad internal
docker exec greenhouse-app curl -s http://localhost:3000/health | jq .

# Esperar ~30s a que inicie
sleep 30
```

---

## Fase 5: Nginx Reverse Proxy

### 5.1 Nueva Configuración Nginx (VPS)

**`/etc/nginx/sites-available/greenhouse`**:
```nginx
# HTTP -> HTTPS Redirect
server {
    listen 80;
    listen [::]:80;
    server_name reimon.dev www.reimon.dev;

    location /.well-known/acme-challenge/ {
        root /var/www/certbot;
    }

    location / {
        return 301 https://$server_name$request_uri;
    }
}

# HTTPS Server
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name reimon.dev www.reimon.dev;

    # SSL Certificates (Let's Encrypt)
    ssl_certificate /etc/letsencrypt/live/reimon.dev/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/reimon.dev/privkey.pem;

    # SSL Configuration
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;
    ssl_prefer_server_ciphers on;

    # Security Headers
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;

    # Logging
    access_log /var/log/nginx/greenhouse_access.log combined;
    error_log /var/log/nginx/greenhouse_error.log;

    # Gzip compression
    gzip on;
    gzip_types text/plain text/css text/xml text/javascript application/javascript application/xml+rss;
    gzip_min_length 1024;

    # ========== GREENHOUSE ROUTES ==========

    # Location: /greenhouse/*
    location /greenhouse/ {
        # Frontend (Nginx dentro del container)
        proxy_pass http://127.0.0.1:3000/;
        
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_cache_bypass $http_upgrade;

        # Client max body size (para uploads si aplica)
        client_max_body_size 10M;

        # Timeouts
        proxy_connect_timeout 60s;
        proxy_send_timeout 60s;
        proxy_read_timeout 60s;
    }

    # API Routes: /greenhouse/api/*
    location /greenhouse/api/ {
        # Backend API
        proxy_pass http://127.0.0.1:8080/api/;
        
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_cache_bypass $http_upgrade;

        client_max_body_size 10M;
    }

    # WebSocket: /greenhouse/socket.io/*
    location /greenhouse/socket.io/ {
        proxy_pass http://127.0.0.1:8080/socket.io/;
        
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "Upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # Important for WebSocket
        proxy_read_timeout 86400;
        proxy_send_timeout 86400;
    }

    # Health check (interno)
    location /greenhouse/health {
        proxy_pass http://127.0.0.1:3000/health;
        access_log off;
    }

    # Static files cache
    location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg|woff|woff2)$ {
        proxy_pass http://127.0.0.1:3000;
        proxy_cache_valid 200 30d;
        expires 30d;
        add_header Cache-Control "public, immutable";
    }

    # Deny other paths
    location ~ /\. {
        deny all;
    }
}
```

### 5.2 Activar Nginx Config

```bash
# En VPS
cd /etc/nginx/sites-available

# Habilitar sitio
sudo ln -sf /etc/nginx/sites-available/greenhouse /etc/nginx/sites-enabled/

# O reemplazar default si es necesario
# sudo rm /etc/nginx/sites-enabled/default
# sudo ln -sf /etc/nginx/sites-available/greenhouse /etc/nginx/sites-enabled/

# Test configuración
sudo nginx -t
# nginx: the configuration file /etc/nginx/nginx.conf syntax is ok
# nginx: configuration will be successful.

# Reload
sudo systemctl reload nginx

# Verificar
sudo systemctl status nginx
```

### 5.3 SSL con Let's Encrypt

```bash
# En VPS (si no tienes certificado aún)

# Instalar certbot
sudo apt install -y certbot python3-certbot-nginx

# Generar certificado
sudo certbot certonly --nginx -d reimon.dev -d www.reimon.dev

# Renovación automática
sudo systemctl enable certbot.timer
sudo systemctl start certbot.timer

# Verificar
sudo certbot certificates
```

### 5.4 Test Nginx

```bash
# Test desde local
curl -I https://reimon.dev/greenhouse/
# HTTP/1.1 200 OK

curl -I https://reimon.dev/greenhouse/api/sensors
# HTTP/1.1 200 OK

# WebSocket test
wscat -c wss://reimon.dev/greenhouse/socket.io
# (debería conectar)
```

---

## Fase 6: Validación Final

### 6.1 Checklist de Validación

```bash
# 1. Frontend accesible
curl -s https://reimon.dev/greenhouse/ | grep -i "greenhouse" | head -1

# 2. Health check
curl -s https://reimon.dev/greenhouse/health | jq .

# 3. Sensor data fetch
curl -s -H "Authorization: Bearer <token>" \
  https://reimon.dev/greenhouse/api/sensors | jq .

# 4. Relay states
curl -s https://reimon.dev/greenhouse/api/relays/states | jq .

# 5. Logs en VPS
docker compose -f docker-compose.prod.yml logs app | tail -20

# 6. MongoDB check
docker exec greenhouse-mongodb mongosh --eval "db.stats()"
```

### 6.2 Simular Conexión ESP32 desde Externa

```bash
# Desde máquina externa (simular ESP32)
TOKEN="tu_token_produccion"

curl -X POST https://reimon.dev/greenhouse/api/sensors \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32_PROD",
    "temperature": 25.5,
    "humidity": 60,
    "soil_moisture": 50
  }'

# Verificar en dashboard
# https://reimon.dev/greenhouse/
```

### 6.3 Monitoreo en VPS

```bash
# Logs en tiempo real
docker compose -f docker-compose.prod.yml logs -f

# Estadísticas de contenedores
docker stats

# Uso de disco
df -h

# Top processes
top

# Conexiones de red
netstat -tulpn | grep LISTEN
```

### 6.4 Troubleshooting

**Si no conecta desde exterior:**
```bash
# 1. Verificar que contenedores están corriendo
docker compose ps

# 2. Verificar puertos listening en localhost
sudo netstat -tulpn | grep LISTEN

# 3. Verificar firewall
sudo ufw status
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp

# 4. Verificar Nginx
sudo nginx -t
sudo systemctl status nginx

# 5. Ver error logs
sudo tail -f /var/log/nginx/greenhouse_error.log
docker compose logs app
```

---

## Fase 7: Mantenimiento

### 7.1 Actualizar Imagen

```bash
# En VPS
cd /opt/greenhouse

# Pull última versión
docker compose pull

# O rebuild si cambios locales
docker compose build --no-cache

# Restart (con downtime mínimo)
docker compose down
docker compose up -d
```

### 7.2 Backup MongoDB

**Script: `backups/backup-mongo.sh`**:
```bash
#!/bin/bash

BACKUP_DIR="/opt/greenhouse/backups/mongo"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_FILE="$BACKUP_DIR/greenhouse_$TIMESTAMP.tar.gz"

mkdir -p $BACKUP_DIR

# Backup
docker exec greenhouse-mongodb mongodump --out /tmp/dump

# Comprimir
tar -czf $BACKUP_FILE /tmp/dump

# Limpiar
docker exec greenhouse-mongodb rm -rf /tmp/dump

# Mantener últimos 7 backups
find $BACKUP_DIR -name "greenhouse_*.tar.gz" -mtime +7 -delete

echo "✅ Backup created: $BACKUP_FILE"
```

**Cron job:**
```bash
# Backup diario a las 2 AM
sudo crontab -e

# Agregar:
0 2 * * * /opt/greenhouse/backups/backup-mongo.sh >> /var/log/greenhouse-backup.log 2>&1
```

### 7.3 Auto-restart en caso de fallo

**`/opt/greenhouse/docker-compose.prod.yml`** - Ya incluye:
```yaml
restart: always  # Reinicia automáticamente si falla
```

### 7.4 Monitoreo Automático

**Script: `scripts/health-monitor.sh`**:
```bash
#!/bin/bash

# Monitorear salud cada 5 minutos
HEALTH_URL="https://reimon.dev/greenhouse/health"
ALERT_EMAIL="tu@email.com"

while true; do
    if ! curl -sf "$HEALTH_URL" > /dev/null; then
        # Alertar
        echo "🚨 Greenhouse health check failed!" | \
          mail -s "Greenhouse Alert" "$ALERT_EMAIL"
        
        # Intentar restart
        docker compose -f /opt/greenhouse/docker-compose.prod.yml restart app
    fi
    
    sleep 300  # 5 minutos
done
```

### 7.5 Logs Centralizados (Opcional)

```bash
# Ver logs últimas 100 líneas
docker compose logs --tail=100 app

# Guardar logs a archivo
docker compose logs > /opt/greenhouse/logs/app.log

# Logs por nivel
docker compose logs app 2>&1 | grep "ERROR\|WARN"
```

---

## Guía Rápida de Comandos

### Local (Netbook)
```bash
# Build
docker compose build

# Iniciar
docker compose up -d

# Parar
docker compose down

# Logs
docker compose logs -f

# Test
bash scripts/test-local.sh
```

### VPS
```bash
# Iniciar
docker compose -f docker-compose.prod.yml up -d

# Parar
docker compose -f docker-compose.prod.yml down

# Restart
docker compose -f docker-compose.prod.yml restart

# Logs
docker compose -f docker-compose.prod.yml logs -f

# Health
curl https://reimon.dev/greenhouse/health

# Backup
bash /opt/greenhouse/backups/backup-mongo.sh
```

---

## Próximos Pasos

1. ✅ **Fase 1-2**: Preparar y testear en local
2. ✅ **Fase 3**: Setup VPS
3. ✅ **Fase 4**: Deploy
4. ✅ **Fase 5**: Nginx config
5. ✅ **Fase 6**: Validar
6. ✅ **Fase 7**: Mantenimiento automático
7. 📱 **Actualizar ESP32**: Cambiar `secrets.h` con nuevo token y VPS_SERVER
8. 🔒 **Seguridad**: Backup tokens, monitoreo, logs

---

**Estado**: 🟢 Listo para implementación  
**Última actualización**: Oct 23, 2024  
**Autor**: Reimon
