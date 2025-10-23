# 🌿 Greenhouse Docker Deployment

> Sistema completo de monitoreo y control de invernadero en contenedores Docker

## 📋 Tabla de Contenidos

1. [Resumen](#resumen)
2. [Estructura](#estructura)
3. [Inicio Rápido](#inicio-rápido-local)
4. [Deployment VPS](#deployment-vps)
5. [Nginx Proxy](#nginx-reverse-proxy)
6. [Monitoreo](#monitoreo--logs)
7. [Referencias](#referencias)

---

## Resumen

Este folder contiene toda la configuración necesaria para desplegar la aplicación Greenhouse completa en Docker:

- **Dockerfile**: Build multi-stage (Frontend + Backend + Nginx)
- **docker-compose.yml**: Desarrollo local con MongoDB integrado
- **docker-compose.prod.yml**: Producción en VPS
- **nginx.conf**: Configuración interna del servidor web
- **start.sh**: Entry point para graceful shutdown
- **Documentación**: Guías completas de deployment

### Componentes Incluidos

```
Container Greenhouse (1 imagen)
├─ Frontend (Vite/React)
│  └─ Nginx :3000
├─ Backend (Node.js)
│  └─ Express + Socket.IO :8080
└─ MongoDB (contenedor separado)
   └─ :27017
```

---

## Estructura

```
docker/
├── Dockerfile                   # Multi-stage build
├── docker-compose.yml          # Dev (local)
├── docker-compose.prod.yml     # Prod (VPS)
├── nginx.conf                  # Nginx config interno
├── start.sh                    # Entry point mejorado
├── .env.example                # Template de variables
├── README.md                   # Este archivo
├── QUICK_START.md              # Inicio rápido
└── ../DOCKER_DEPLOYMENT_PLAN.md # Plan detallado (7 fases)
```

---

## Inicio Rápido (Local)

### Requisitos
```bash
docker --version        # v24+
docker compose version  # v2.0+
```

### 1. Build
```bash
cd /home/reimon/greenhouse/docker
docker compose build

# Ver progreso
# Esperar ~3-5 minutos (primera vez)
```

### 2. Iniciar
```bash
docker compose up -d

# Verificar estado
docker compose ps
# STATUS: healthy ✅
```

### 3. Testear
```bash
# Frontend
open http://localhost:3000

# API
curl http://localhost:3000/health | jq .

# Ver logs
docker compose logs -f app
```

### 4. Acceder Dashboard
- **URL**: http://localhost:3000
- **Usuario**: N/A (sin autenticación en dev)
- **Token ESP32**: `dev_token_minimum_32_characters_required_please_change_me_asap`

---

## Deployment VPS

### Pre-requisitos VPS
- Ubuntu 20.04+
- SSH access
- ~2GB RAM mínimo
- ~5GB disco para datos

### Paso 1: Instalar Docker en VPS

```bash
ssh user@reimon.dev

# Instalar Docker
curl -fsSL https://get.docker.com | sh

# Instalar Docker Compose
sudo curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-$(uname -s)-$(uname -m)" \
  -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose

# Verificar
docker --version
docker compose version
```

### Paso 2: Preparar estructura en VPS

```bash
# En VPS
sudo mkdir -p /opt/greenhouse
sudo chown $USER:$USER /opt/greenhouse
cd /opt/greenhouse

mkdir -p {config,data/mongodb,logs,backups}
```

### Paso 3: Copiar proyecto a VPS

```bash
# Desde local (netbook)
scp -r /home/reimon/greenhouse user@reimon.dev:/opt/greenhouse/

# O clonar si está en GitHub
# ssh user@reimon.dev
# git clone https://github.com/usuario/greenhouse.git /opt/greenhouse
```

### Paso 4: Configurar credenciales

```bash
# En VPS
cd /opt/greenhouse

# Generar token seguro
TOKEN=$(openssl rand -hex 32)
echo "Tu token: $TOKEN"

# Crear .env.production
cat > docker/.env.production << EOF
PORT=8080
NODE_ENV=production
MONGODB_URI=mongodb://mongodb:27017/greenhouse
ALLOWED_ORIGINS=https://reimon.dev
RATE_LIMIT_WINDOW_MS=900000
RATE_LIMIT_MAX_REQUESTS=5000
ESP32_AUTH_TOKEN=$TOKEN
LOG_LEVEL=info
EOF

chmod 600 docker/.env.production
```

### Paso 5: Build y Deploy

```bash
# En VPS
cd /opt/greenhouse/docker

# Build
docker compose -f docker-compose.prod.yml build

# Iniciar
docker compose -f docker-compose.prod.yml up -d

# Verificar
docker compose -f docker-compose.prod.yml ps

# Ver logs
docker compose -f docker-compose.prod.yml logs -f app
```

### Paso 6: Verificar servicio

```bash
# En VPS (local)
curl http://127.0.0.1:3000/health
curl http://127.0.0.1:8080/api/sensors

# El servicio debe estar accesible internamente
```

---

## Nginx Reverse Proxy

### Crear configuración Nginx

Crear `/etc/nginx/sites-available/greenhouse`:

```nginx
# HTTP -> HTTPS Redirect
server {
    listen 80;
    server_name reimon.dev www.reimon.dev;
    
    location /.well-known/acme-challenge/ {
        root /var/www/certbot;
    }
    
    location / {
        return 301 https://$server_name$request_uri;
    }
}

# HTTPS
server {
    listen 443 ssl http2;
    server_name reimon.dev www.reimon.dev;
    
    ssl_certificate /etc/letsencrypt/live/reimon.dev/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/reimon.dev/privkey.pem;
    
    # Security
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;
    
    add_header Strict-Transport-Security "max-age=31536000" always;
    
    # Logs
    access_log /var/log/nginx/greenhouse_access.log;
    error_log /var/log/nginx/greenhouse_error.log;
    
    # Gzip
    gzip on;
    gzip_types text/plain text/css text/javascript application/javascript;
    
    # ===== GREENHOUSE ROUTES =====
    
    # Frontend
    location /greenhouse/ {
        proxy_pass http://127.0.0.1:3000/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
    
    # API
    location /greenhouse/api/ {
        proxy_pass http://127.0.0.1:8080/api/;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
    
    # WebSocket
    location /greenhouse/socket.io/ {
        proxy_pass http://127.0.0.1:8080/socket.io/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "Upgrade";
        proxy_set_header Host $host;
        proxy_read_timeout 86400;
    }
}
```

### Activar Nginx

```bash
# En VPS
sudo ln -sf /etc/nginx/sites-available/greenhouse /etc/nginx/sites-enabled/

# Test
sudo nginx -t

# Reload
sudo systemctl reload nginx
```

### SSL con Let's Encrypt

```bash
# En VPS
sudo apt install certbot python3-certbot-nginx

# Generar certificado
sudo certbot certonly --nginx -d reimon.dev

# Auto-renewal
sudo systemctl enable certbot.timer
sudo systemctl start certbot.timer
```

---

## Acceso Producción

### URL
```
https://reimon.dev/greenhouse/
```

### Rutas Disponibles
```
Endpoint                    Proxy a
────────────────────────────────────────────
/greenhouse/                → Nginx :3000 (Frontend)
/greenhouse/api/sensors     → Backend :8080/api/sensors
/greenhouse/socket.io/      → WebSocket :8080/socket.io/
/greenhouse/health          → Health check
```

---

## Monitoreo & Logs

### Ver Logs

```bash
# Local
docker compose logs -f app

# VPS
docker compose -f docker-compose.prod.yml logs -f app

# Solo errores
docker compose logs app 2>&1 | grep ERROR
```

### Health Check

```bash
# Local
curl http://localhost:3000/health | jq .

# VPS (desde local)
curl https://reimon.dev/greenhouse/health | jq .

# Esperado:
# {
#   "status": "ok",
#   "database": { "status": "connected" },
#   "websocket": { "totalConnections": 1 },
#   "memory": { "rss": 150 }
# }
```

### Ver Recursos

```bash
# CPU, RAM, I/O
docker stats greenhouse-app

# Procesos dentro del container
docker top greenhouse-app

# Volúmenes
docker volume ls
```

### Limpiar

```bash
# Parar (sin eliminar)
docker compose stop

# Parar y eliminar
docker compose down

# Eliminar TODO (datos también)
docker compose down -v

# Limpiar imágenes no usadas
docker image prune -a
```

---

## Troubleshooting

### Container no inicia
```bash
# Ver error exacto
docker compose logs app

# Verificar MongoDB
docker compose logs mongodb

# Reintentar
docker compose restart
```

### Port en uso
```bash
# Encontrar quién usa el puerto
lsof -i :3000

# O cambiar en docker-compose.yml:
# ports:
#   - "3001:3000"
```

### MongoDB no responde
```bash
# Esperar más (primera vez tarda)
docker compose logs -f mongodb

# Resetear
docker compose down -v
docker compose up -d
```

### Nginx errores 502/503
```bash
# Verificar backend corriendo
docker compose ps app

# Ver logs de error
docker compose logs app

# Restart backend
docker compose restart app
```

---

## Backup & Restauración

### Backup MongoDB

```bash
# Dump database
docker exec greenhouse-mongodb mongodump --out /tmp/dump

# Comprimir
tar -czf greenhouse_backup_$(date +%s).tar.gz /tmp/dump

# Guardar en lugar seguro
cp greenhouse_backup_*.tar.gz /backups/
```

### Restaurar MongoDB

```bash
# Extraer
tar -xzf greenhouse_backup_*.tar.gz

# Restore
docker exec greenhouse-mongodb mongorestore /tmp/dump
```

---

## Actualizar Aplicación

### Build nueva versión

```bash
# Código actualizado en local
docker compose build --no-cache

# Test local
docker compose up -d
# ... test ...

# Enviar a VPS
docker tag greenhouse-docker-app tu-usuario/greenhouse:v1.1
docker push tu-usuario/greenhouse:v1.1
```

### Deploy en VPS

```bash
# En VPS
docker pull tu-usuario/greenhouse:v1.1

# Actualizar docker-compose.prod.yml con nueva versión
# Luego:
docker compose -f docker-compose.prod.yml down
docker compose -f docker-compose.prod.yml up -d

# Verificar
docker compose -f docker-compose.prod.yml logs app
```

---

## Seguridad

### Checklist

- ✅ `.env` y `.env.production` en `.gitignore`
- ✅ Token ESP32 > 32 caracteres
- ✅ MongoDB sin puerto expuesto en producción
- ✅ HTTPS/SSL en producción
- ✅ Rate limiting habilitado
- ✅ Logs monitoreados regularmente

### Token Seguro

```bash
# Generar nuevo token
openssl rand -hex 32

# Usar en .env.production
ESP32_AUTH_TOKEN=<salida_del_comando_anterior>

# Actualizar en ESP32 secrets.h también
```

---

## Archivos de Configuración Importantes

### Local (.gitignore ✅)
- `docker/.env`
- `backend-websocket-update/.env`

### VPS (crear manualmente)
- `/opt/greenhouse/docker/.env.production`

### Nginx
- `/etc/nginx/sites-available/greenhouse`

### SSL
- `/etc/letsencrypt/live/reimon.dev/`

---

## Documentación Completa

- **`QUICK_START.md`** - Inicio en 5 minutos
- **`../DOCKER_DEPLOYMENT_PLAN.md`** - Plan detallado (7 fases)
- **`../PROJECT_SUMMARY.md`** - Visión general del proyecto

---

## Problemas Frecuentes

| Problema | Solución |
|----------|----------|
| Build falla | `docker compose down -v && docker compose build --no-cache` |
| Port 3000 en uso | Cambiar en `docker-compose.yml` ports |
| MongoDB no conecta | Esperar 30s, ver logs: `docker logs mongodb` |
| API retorna 502 | Verificar backend: `docker compose ps` |
| WebSocket desconecta | Verificar rate limiting, firewall |
| Certificado SSL expirado | `sudo certbot renew` |

---

## Comandos Rápidos

```bash
# DEV LOCAL
docker compose build           # Build
docker compose up -d           # Iniciar
docker compose down            # Parar
docker compose logs -f         # Logs

# PROD VPS
cd /opt/greenhouse/docker
docker compose -f docker-compose.prod.yml up -d
docker compose -f docker-compose.prod.yml logs -f
docker compose -f docker-compose.prod.yml restart

# NGINX
sudo nginx -t                  # Test config
sudo systemctl reload nginx    # Reload
sudo systemctl status nginx    # Estado

# CERTBOT
sudo certbot renew             # Renovar SSL
sudo certbot certificates      # Ver certificados
```

---

## Support

- 📖 Documentación: Ver archivos `.md` en este directorio
- 🐛 Bugs: Revisar logs con `docker compose logs`
- 📝 Logs: `/var/log/nginx/` en VPS
- 🔍 Health: `curl http://localhost:3000/health`

---

**Versión**: 1.0  
**Última actualización**: Oct 23, 2024  
**Mantenedor**: Reimon
