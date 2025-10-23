# 📦 Plan de Despliegue Docker - Resumen Ejecutivo

## 🎯 Objetivo General

Containerizar la aplicación Greenhouse completa (Frontend + Backend + MongoDB) en Docker y deployarla en tu VPS `reimon.dev` con proxy reverso Nginx.

---

## 📊 Arquitectura Final

```
Tu Netbook (Desarrollo)          Tu VPS (Producción)
══════════════════════════════════════════════════════════

Docker Containers              Docker Containers
┌─────────────────┐            ┌──────────────────────┐
│ Nginx :3000     │            │ Nginx :3000          │
│ Backend :8080   │            │ Backend :8080        │
│ MongoDB :27017  │            │ MongoDB :27017       │
└────────┬────────┘            └──────────┬───────────┘
         │                                │
         └──────→ HTTP/HTTPS ←────────┬───┘
        :3000,:8080              Nginx proxy
                              /etc/nginx/sites-available/
                                    greenhouse
                                      ↓
                          https://reimon.dev/greenhouse/
                                (HTTPS + SSL)
```

---

## 📋 Checklist Rápido

### Fase 1: Local (Netbook) - ~30 minutos
```bash
cd /home/reimon/greenhouse/docker

# 1. Build imagen
docker compose build

# 2. Iniciar
docker compose up -d

# 3. Test
curl http://localhost:3000
docker compose logs -f
```

**Archivos modificados/creados:**
- ✅ `docker/Dockerfile` - Build multi-stage mejorado
- ✅ `docker/docker-compose.yml` - Dev con healthchecks
- ✅ `docker/docker-compose.prod.yml` - Producción
- ✅ `docker/start.sh` - Entry point mejorado
- ✅ `docker/.env.example` - Template variables
- ✅ `docker/README.md` - Documentación

### Fase 2: VPS Setup - ~15 minutos
```bash
ssh user@reimon.dev

# 1. Instalar Docker
curl -fsSL https://get.docker.com | sh

# 2. Preparar directorios
sudo mkdir -p /opt/greenhouse
sudo chown $USER:$USER /opt/greenhouse

# 3. Copiar proyecto
scp -r /home/reimon/greenhouse user@reimon.dev:/opt/greenhouse/
```

### Fase 3: Deploy VPS - ~10 minutos
```bash
# En VPS
cd /opt/greenhouse/docker

# Generar token seguro
TOKEN=$(openssl rand -hex 32)
echo $TOKEN

# Crear .env.production con el token

# Build y deploy
docker compose -f docker-compose.prod.yml build
docker compose -f docker-compose.prod.yml up -d

# Verificar
docker compose -f docker-compose.prod.yml logs -f
```

### Fase 4: Nginx Config - ~10 minutos
```bash
# En VPS
# Crear: /etc/nginx/sites-available/greenhouse
# (ver archivo greenhouse en docker/)

sudo ln -sf /etc/nginx/sites-available/greenhouse /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx

# SSL
sudo apt install certbot python3-certbot-nginx
sudo certbot certonly --nginx -d reimon.dev
```

### Fase 5: Validación - ~5 minutos
```bash
# Test
curl https://reimon.dev/greenhouse/health
# Acceder: https://reimon.dev/greenhouse/

# Test API
curl https://reimon.dev/greenhouse/api/sensors

# Logs
docker compose -f docker-compose.prod.yml logs -f
```

---

## 📁 Estructura Creada

```
/home/reimon/greenhouse/
├── docker/
│   ├── Dockerfile ........................ Multi-stage build
│   ├── docker-compose.yml ............... Dev local
│   ├── docker-compose.prod.yml .......... Producción
│   ├── nginx.conf ....................... Config Nginx interno
│   ├── start.sh ......................... Entry point mejorado
│   ├── .env.example ..................... Template variables
│   ├── README.md ........................ Documentación (¡LEE!)
│   ├── QUICK_START.md ................... Inicio rápido
│   └── greenhouse ........................ Nginx config para VPS
│
├── DOCKER_DEPLOYMENT_PLAN.md ........... Plan detallado (7 fases)
├── PROJECT_SUMMARY.md ................. Visión general proyecto
└── .gitignore .......................... Actualizado con secrets
```

---

## 🔐 Seguridad - MUY IMPORTANTE

### Nunca commitear:
- `.env` files
- `.env.production`
- `secrets.h` del ESP32
- Cualquier token o contraseña

### Esto lo hace `.gitignore`:
```
docker/.env
docker/.env.production
backend-websocket-update/.env
docker/config/secrets/
```

### Generar token seguro:
```bash
openssl rand -hex 32
# Usar en .env.production y en ESP32/secrets.h
```

---

## 🚀 Próximos Pasos Inmediatos

### 1. Test Local (PRIMERO!)
```bash
cd /home/reimon/greenhouse/docker
docker compose build
docker compose up -d
# Ir a http://localhost:3000
# Ver logs: docker compose logs -f
```

### 2. Leer Documentación
- `docker/README.md` - Toda la info
- `docker/QUICK_START.md` - Inicio rápido
- `DOCKER_DEPLOYMENT_PLAN.md` - Plan completo

### 3. Preparar VPS
- Conectar SSH a reimon.dev
- Instalar Docker
- Crear estructura `/opt/greenhouse`

### 4. Deploy en VPS
- Generar `.env.production` con token único
- Build imagen
- Iniciar contenedores
- Verificar logs

### 5. Nginx + SSL
- Crear config en `/etc/nginx/sites-available/greenhouse`
- Configurar SSL con Let's Encrypt
- Test desde público

### 6. Actualizar ESP32
- Cambiar `API_TOKEN` en `secrets.h` (usar token del VPS)
- Cambiar `VPS_SERVER` a `reimon.dev`
- Compilar y subir firmware

---

## 📞 Comandos Esenciales

### Local
```bash
# Build
docker compose build

# Iniciar
docker compose up -d

# Parar
docker compose down

# Logs
docker compose logs -f app
docker compose logs app -n 100

# Test
curl http://localhost:3000/health
```

### VPS
```bash
cd /opt/greenhouse/docker

# Deploy
docker compose -f docker-compose.prod.yml up -d

# Ver estado
docker compose -f docker-compose.prod.yml ps

# Logs
docker compose -f docker-compose.prod.yml logs -f

# Restart
docker compose -f docker-compose.prod.yml restart

# Health check
curl http://127.0.0.1:3000/health
```

### Nginx
```bash
# Test config
sudo nginx -t

# Reload
sudo systemctl reload nginx

# Ver estado
sudo systemctl status nginx

# Logs
sudo tail -f /var/log/nginx/greenhouse_error.log
```

---

## 🎁 Lo que ya está listo

✅ **Dockerfile optimizado** - Multi-stage, pequeño, eficiente  
✅ **docker-compose.yml** - Dev con MongoDB integrado  
✅ **docker-compose.prod.yml** - Producción segura  
✅ **start.sh mejorado** - Graceful shutdown, mejor logging  
✅ **nginx.conf** - Config internal del container  
✅ **Documentación completa** - 3 archivos .md  
✅ **Plan de 7 fases** - Paso a paso  
✅ **.gitignore actualizado** - Protege secretos  

---

## ⚠️ Notas Importantes

1. **Testing local ANTES de VPS** - Crucial para evitar problemas
2. **Token seguro** - Generar con `openssl rand -hex 32`
3. **Backup nginx.conf actual** - Antes de cambiar configuración
4. **SSL automático** - Let's Encrypt configurado en plan
5. **MongoDB TTL 30 días** - Datos se auto-limpian
6. **Rate limiting** - 120 eventos/minuto por socket

---

## 📈 Estimación de Tiempo Total

| Fase | Tiempo | Complejidad |
|------|--------|-------------|
| 1. Test Local | 30 min | ⭐ Baja |
| 2. VPS Setup | 15 min | ⭐ Baja |
| 3. Deploy | 15 min | ⭐ Baja |
| 4. Nginx + SSL | 15 min | ⭐⭐ Media |
| 5. ESP32 Update | 10 min | ⭐ Baja |
| **TOTAL** | **~90 min** | **Manejable** |

---

## 🎓 Recursos

### Documentación en el Proyecto
- `docker/README.md` - Completa
- `docker/QUICK_START.md` - Rápida
- `DOCKER_DEPLOYMENT_PLAN.md` - Detallada (7 fases)
- `PROJECT_SUMMARY.md` - Visión general

### Referencias Externas
- Docker: https://docs.docker.com
- Nginx: https://nginx.org/en/docs/
- Let's Encrypt: https://certbot.eff.org/

---

## ✅ Validación Final

Cuando todo esté listo:

```bash
# 1. Frontend accesible
curl https://reimon.dev/greenhouse/

# 2. Health check OK
curl https://reimon.dev/greenhouse/health | jq .

# 3. API funcionando
curl https://reimon.dev/greenhouse/api/sensors | jq .

# 4. Logs sin errores
docker compose -f docker-compose.prod.yml logs app | grep ERROR

# 5. ESP32 conecta
# Verificar en logs del backend: "device:auth_success"
```

---

## 🆘 Soporte Rápido

**Si algo no funciona:**

1. Ver logs: `docker compose logs -f app`
2. Verificar conectividad: `curl http://localhost:3000/health`
3. Revisar .env: `cat docker/.env.production | grep -v "^#"`
4. Restart: `docker compose down && docker compose up -d`
5. Leer: `docker/README.md` sección Troubleshooting

---

## 🎯 Próximo Paso Inmediato

👉 **LEE `docker/QUICK_START.md`** - Te da el inicio en 5 minutos

Luego:
1. Haz test local
2. Lee el plan completo
3. Deploy en VPS
4. Disfruta tu Greenhouse en la nube ☁️🌿

---

**Estado**: 🟢 Listo para implementar  
**Fecha**: Oct 23, 2024  
**Creado por**: Plan generado automáticamente
