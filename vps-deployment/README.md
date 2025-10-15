# Greenhouse IoT System - VPS Deployment Guide

## 📋 Arquitectura del Sistema

```
ESP32 Greenhouse ──┬──> POST /api/sensors (cada 30s)
                   ├──> POST /api/logs
                   └──> GET /api/relays/states (polling)

VPS Ubuntu 24.04 ──┬──> MongoDB (puerto 27017)
                   ├──> Node.js API (puerto 3000)
                   └──> nginx (puertos 80/443)
                        └──> React Frontend (Material UI)
```

## 🚀 Paso 1: Conectarse al VPS

```bash
ssh -p 5591 root@168.181.185.42
# Password: 8nHVC)M()DN1ka
```

## 📦 Paso 2: Instalar MongoDB

Copia y pega este script completo:

```bash
#!/bin/bash
set -e

echo "=== Actualizando sistema ==="
apt-get update
apt-get upgrade -y

echo "=== Instalando dependencias ==="
apt-get install -y curl gnupg

echo "=== Instalando MongoDB 8.0 ==="
curl -fsSL https://www.mongodb.org/static/pgp/server-8.0.asc | \
  gpg -o /usr/share/keyrings/mongodb-server-8.0.gpg --dearmor

echo "deb [ arch=amd64,arm64 signed-by=/usr/share/keyrings/mongodb-server-8.0.gpg ] https://repo.mongodb.org/apt/ubuntu noble/mongodb-org/8.0 multiverse" | \
  tee /etc/apt/sources.list.d/mongodb-org-8.0.list

apt-get update
apt-get install -y mongodb-org

systemctl start mongod
systemctl enable mongod
sleep 5

echo "=== Creando usuario administrador ==="
mongosh --eval '
use admin
db.createUser({
  user: "greenhouse_admin",
  pwd: "GH2025_SecurePass!",
  roles: [
    {role: "userAdminAnyDatabase", db: "admin"},
    {role: "readWriteAnyDatabase", db: "admin"}
  ]
})
'

echo "=== Habilitando autenticación ==="
sed -i 's/#security:/security:\n  authorization: enabled/' /etc/mongod.conf
systemctl restart mongod
sleep 3

echo "=== Creando database greenhouse ==="
mongosh -u greenhouse_admin -p 'GH2025_SecurePass!' --authenticationDatabase admin --eval '
use greenhouse
db.createCollection("sensor_readings")
db.createCollection("relay_states")
db.createCollection("rules")
db.createCollection("system_logs")

db.sensor_readings.createIndex({timestamp: -1})
db.sensor_readings.createIndex({device_id: 1, timestamp: -1})
db.relay_states.createIndex({relay_id: 1, timestamp: -1})
db.system_logs.createIndex({timestamp: -1})
db.rules.createIndex({relay_id: 1})

print("✓ Database greenhouse creada con éxito")
'

echo "=== ✓ MongoDB configurado exitosamente ==="
```

## 🟢 Paso 3: Instalar Node.js y nginx

```bash
#!/bin/bash
set -e

curl -fsSL https://deb.nodesource.com/setup_20.x | bash -
apt-get install -y nodejs nginx

npm install -g pm2

mkdir -p /opt/greenhouse-api
mkdir -p /opt/greenhouse-frontend

ufw allow 5591/tcp
ufw allow 80/tcp
ufw allow 443/tcp
ufw --force enable

echo "=== ✓ Node.js y nginx instalados ==="
node --version
npm --version
nginx -v
```

## 📂 Paso 4: Subir el código del backend

En tu máquina local (donde estás ahora):

```bash
cd /home/reimon/greenhouse/vps-deployment

# Comprimir el backend
tar czf backend.tar.gz backend/

# Subir al VPS
scp -P 5591 backend.tar.gz root@168.181.185.42:/opt/

# Conectarse al VPS nuevamente
ssh -p 5591 root@168.181.185.42

# Descomprimir en el VPS
cd /opt
tar xzf backend.tar.gz
cd backend

# Instalar dependencias
npm install --production

# Crear archivo .env
cat > .env << 'EOF'
NODE_ENV=production
PORT=3000
MONGODB_URI=mongodb://greenhouse_admin:GH2025_SecurePass!@localhost:27017/greenhouse?authSource=admin
API_TOKEN=your_secure_token_change_this
ALLOWED_ORIGINS=http://168.181.185.42,https://168.181.185.42
RATE_LIMIT_WINDOW_MS=900000
RATE_LIMIT_MAX_REQUESTS=100
EOF

# Iniciar con PM2
pm2 start server.js --name greenhouse-api
pm2 save
pm2 startup

# Ver logs
pm2 logs greenhouse-api
```

## 🧪 Paso 5: Verificar que la API funciona

```bash
# Health check
curl http://localhost:3000/health

# Debería devolver:
# {"status":"ok","timestamp":"...","uptime":...,"mongodb":"connected"}
```

## 🌐 Paso 6: Configurar nginx

```bash
cat > /etc/nginx/sites-available/greenhouse << 'EOF'
server {
    listen 80;
    server_name 168.181.185.42;

    # API proxy
    location /api/ {
        proxy_pass http://localhost:3000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_cache_bypass $http_upgrade;
    }

    # Health check
    location /health {
        proxy_pass http://localhost:3000;
    }

    # Frontend (temporalmente redirige al API)
    location / {
        proxy_pass http://localhost:3000;
    }
}
EOF

ln -sf /etc/nginx/sites-available/greenhouse /etc/nginx/sites-enabled/
rm -f /etc/nginx/sites-enabled/default
nginx -t
systemctl restart nginx
```

## ✅ Verificación Final

Desde tu navegador o terminal local:

```bash
curl http://168.181.185.42/health
```

Deberías ver:
```json
{"status":"ok","timestamp":"2025-10-14T...","uptime":123,"mongodb":"connected"}
```

## 📊 Endpoints de la API

### Para el ESP32:
- `POST /api/sensors` - Enviar lecturas de sensores
- `POST /api/relays/:id/state` - Cambiar estado de relay
- `GET /api/relays/states` - Obtener estados actuales
- `GET /api/rules?relay_id=0` - Obtener reglas
- `POST /api/logs` - Enviar logs

### Para el Frontend:
- `GET /api/sensors?limit=100` - Últimas 100 lecturas
- `GET /api/sensors/latest` - Última lectura
- `GET /api/relays/states` - Estados de todos los relays
- `GET /api/rules` - Todas las reglas
- `POST /api/rules` - Crear regla
- `PUT /api/rules/:id` - Actualizar regla
- `DELETE /api/rules/:id` - Eliminar regla
- `GET /api/logs?limit=50` - Últimos 50 logs

## 🔐 Credenciales

**MongoDB:**
- Usuario: `greenhouse_admin`
- Password: `GH2025_SecurePass!`
- Database: `greenhouse`
- URI: `mongodb://greenhouse_admin:GH2025_SecurePass!@localhost:27017/greenhouse?authSource=admin`

**VPS:**
- IP: `168.181.185.42`
- Puerto SSH: `5591`
- Usuario: `root`

## 📝 Próximos Pasos

1. ✅ Backend API funcionando
2. ⏳ Modificar firmware ESP32 para enviar datos al VPS
3. ⏳ Crear frontend React con Material UI
4. ⏳ Configurar SSL con Let's Encrypt

---

## 🐛 Troubleshooting

### Ver logs de la API:
```bash
pm2 logs greenhouse-api
```

### Reiniciar la API:
```bash
pm2 restart greenhouse-api
```

### Verificar MongoDB:
```bash
systemctl status mongod
mongosh -u greenhouse_admin -p 'GH2025_SecurePass!' --authenticationDatabase admin
```

### Ver logs de nginx:
```bash
tail -f /var/log/nginx/error.log
tail -f /var/log/nginx/access.log
```
