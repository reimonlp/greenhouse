# 🚀 Quick Start - Docker Deployment

## Local Setup (5 minutos)

### 1️⃣ Requisitos
```bash
# Verificar que tienes instalado
docker --version      # v24+
docker compose version  # v2.0+
git --version
```

### 2️⃣ Clonar/Preparar Proyecto
```bash
cd /home/reimon
# Ya tienes el repo en greenhouse/
cd greenhouse
```

### 3️⃣ Build de Imagen
```bash
cd greenhouse/docker
docker compose build

# Ver imagen creada
docker images | grep greenhouse
```

### 4️⃣ Iniciar Contenedores
```bash
# Desde greenhouse/docker
docker compose up -d

# Verificar
docker compose ps
# Status debería ser "healthy"
```

### 5️⃣ Testear
```bash
# Frontend
curl http://localhost:3000
# Output: <html>...</html>

# API Health
curl http://localhost:3000/health | jq .

# Ver logs
docker compose logs -f app
```

### 📱 Acceder Dashboard
- **Frontend**: http://localhost:3000
- **Backend API**: http://localhost:8080
- **MongoDB**: localhost:27017 (interno)

---

## Troubleshooting Local

### ❌ Build falla: "npm ci failed"
```bash
# Limpiar cache
docker compose down -v
docker system prune -a

# Reintentar build
docker compose build --no-cache
```

### ❌ Contenedores no inician
```bash
# Ver logs de error
docker compose logs app

# Verificar MongoDB
docker compose logs mongodb

# Restart
docker compose restart
```

### ❌ Port 3000 ya en uso
```bash
# Encontrar qué usa el puerto
sudo lsof -i :3000

# O cambiar puerto en docker-compose.yml
# ports:
#   - "3001:3000"
```

### ❌ MongoDB no conecta
```bash
# Verificar que MongoDB inició
docker compose logs mongodb

# Esperar más (primera vez tarda)
docker compose logs -f mongodb

# Resetear volúmenes
docker compose down -v
docker compose up -d
```

---

## Parar/Continuar

```bash
# Parar (sin eliminar datos)
docker compose stop

# Reanudar
docker compose start

# Reiniciar
docker compose restart

# Parar TODO (elimina contenedores pero no volúmenes)
docker compose down

# Eliminar TODO incluyendo datos
docker compose down -v
```

---

## Para ESP32 (Local Testing)

```bash
# Token para usar en ESP32
TOKEN="dev_token_minimum_32_characters_required_please_change_me_asap"

# Simular envío de sensor
curl -X POST http://localhost:8080/api/sensors \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32_TEST",
    "temperature": 25.5,
    "humidity": 60,
    "soil_moisture": 50
  }'

# Ver datos
curl http://localhost:8080/api/sensors/latest | jq .
```

---

## VPS Deployment (Cuando esté listo)

Ver `DOCKER_DEPLOYMENT_PLAN.md` - Fase 3-5

### Resumen VPS:
1. **SSH a VPS**: `ssh user@reimon.dev`
2. **Instalar Docker**: `curl -fsSL https://get.docker.com | sh`
3. **Copiar proyecto**: `scp -r greenhouse user@reimon.dev:/opt/`
4. **Configurar `.env.production`**: Token único y MONGODB_URI
5. **Build**: `docker compose -f docker-compose.prod.yml build`
6. **Deploy**: `docker compose -f docker-compose.prod.yml up -d`
7. **Nginx**: Configurar proxy reverso en `/etc/nginx/sites-available/greenhouse`
8. **SSL**: `sudo certbot certonly --nginx -d reimon.dev`
9. **Acceder**: https://reimon.dev/greenhouse/

---

## Comandos Útiles

```bash
# Logs en tiempo real
docker compose logs -f

# Logs de servicio específico
docker compose logs -f app
docker compose logs -f mongodb

# Ejecutar comando en contenedor
docker compose exec app curl http://localhost:3000/health
docker compose exec mongodb mongosh

# Ver recursos
docker stats greenhouse-app

# Listar todos los contenedores
docker ps -a

# Inspeccionar contenedor
docker inspect greenhouse-app

# Copiar archivos desde container
docker cp greenhouse-app:/app/backend/package.json .

# Reset completo
docker compose down -v --remove-orphans
docker system prune -a
docker compose up -d
```

---

## Próximos Pasos

- ✅ Test local completo
- 📋 Leer `DOCKER_DEPLOYMENT_PLAN.md` completo
- 🔧 Preparar VPS (instalar Docker)
- 🚀 Deploy en producción
- 🔐 Configurar Nginx con SSL
- 🔄 Automatizar backups

---

**Versión**: 1.0  
**Última actualización**: Oct 23, 2024
