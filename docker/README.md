# 🌿 Greenhouse Docker

Sistema de monitoreo y control de invernadero en contenedores Docker.

## 🚀 Inicio Rápido

### Desarrollo Local
```bash
# Build y start
docker compose build
docker compose up -d

# Verificar
curl http://localhost:3000/health
```

### Producción VPS
```bash
# Configurar Nginx
sudo cp nginx-prod.conf /etc/nginx/sites-available/greenhouse
sudo ln -sf /etc/nginx/sites-available/greenhouse /etc/nginx/sites-enabled/

# Generar .env.production
./generate_env.sh

# Deploy
docker compose -f docker-compose.prod.yml up -d
```

## 📊 Arquitectura

| Componente | Puerto | Descripción |
|------------|--------|-------------|
| **Express** | 3000 | Frontend + API + WebSocket |
| **MongoDB** | 27017 | Base de datos (solo desarrollo) |

## 🔧 Configuración

### Desarrollo
- **Frontend**: http://localhost:3000
- **API**: http://localhost:3000/api/*
- **WebSocket**: http://localhost:3000/socket.io/

### Producción
- **Frontend**: https://reimon.dev/greenhouse/
- **API**: https://reimon.dev/greenhouse/api/*
- **WebSocket**: https://reimon.dev/greenhouse/socket.io/

## 📁 Archivos

- `docker-compose.yml` - Desarrollo local
- `docker-compose.prod.yml` - Producción VPS
- `Dockerfile` - Build multi-stage
- `nginx-prod.conf` - Configuración Nginx
- `generate_env.sh` - Script para .env.production
- `DEPLOYMENT_PORTS.md` - Documentación de puertos

## 🧪 Testing

```bash
# Health check
curl http://localhost:3000/health

# API test
curl http://localhost:3000/api/sensors

# ESP32 simulation
TOKEN="dev_token_minimum_32_characters_required_please_change_me_asap"
curl -X POST http://localhost:3000/api/sensors \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"device_id": "ESP32_TEST", "temperature": 25.5, "humidity": 60}'
```

## 🔒 Seguridad

- **Desarrollo**: MongoDB expuesto para debugging
- **Producción**: MongoDB solo interno, Express solo localhost
- **Nginx**: Proxy con SSL, autenticación ESP32

## 📝 Comandos Útiles

```bash
# Logs
docker compose logs -f

# Restart
docker compose restart

# Clean
docker compose down -v

# Stats
docker stats
```
