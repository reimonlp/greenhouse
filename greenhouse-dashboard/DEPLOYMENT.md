# Greenhouse Dashboard - Deployment Guide

## 📦 Build de Producción

El frontend ya está compilado y listo para deployment en:
```
greenhouse-frontend.tar.gz (250KB)
```

## 🚀 Deployment al VPS

### 1. Subir archivos al VPS

```bash
scp -P 5591 greenhouse-frontend.tar.gz root@168.181.185.42:/tmp/
```

### 2. Conectarse al VPS

```bash
ssh -p 5591 root@168.181.185.42
```

### 3. Desplegar en el VPS

```bash
# Crear directorio para el frontend
sudo mkdir -p /opt/greenhouse-frontend

# Extraer archivos
sudo tar -xzf /tmp/greenhouse-frontend.tar.gz -C /opt/greenhouse-frontend

# Ajustar permisos
sudo chown -R www-data:www-data /opt/greenhouse-frontend

# Reiniciar nginx
sudo systemctl restart nginx
```

### 4. Verificar configuración de nginx

El archivo `/etc/nginx/sites-available/greenhouse` debe tener:

```nginx
server {
    listen 80;
    server_name 168.181.185.42;

    # Frontend React
    location / {
        root /opt/greenhouse-frontend;
        index index.html;
        try_files $uri $uri/ /index.html;
    }

    # API Backend
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
}
```

Si necesitas actualizar nginx:

```bash
sudo nano /etc/nginx/sites-available/greenhouse
sudo nginx -t
sudo systemctl reload nginx
```

## 🔍 Verificación

Después del deployment, verifica que todo funcione:

### 1. Frontend accesible
```bash
curl -I http://168.181.185.42/
# Debería retornar 200 OK
```

### 2. API funcionando
```bash
curl http://168.181.185.42/health
# Debería retornar {"status":"ok","mongodb":"connected"}
```

### 3. Datos de sensores
```bash
curl http://168.181.185.42/api/sensors/latest
# Debería retornar los datos del ESP32
```

### 4. Abrir en navegador
```
http://168.181.185.42/
```

Deberías ver:
- ✅ Tarjetas de sensores (Temperatura, Humedad, Humedad Suelo)
- ✅ Controles de relés (Luces, Ventilador, Bomba, Calefactor)
- ✅ Gráficos históricos
- ✅ Gestor de reglas
- ✅ Visor de logs

## 📊 Componentes del Dashboard

### SensorCard
- Muestra valores en tiempo real
- Indicadores visuales de estado (colores según rango)
- Actualización automática cada 5 segundos

### RelayControl
- Switches para controlar cada relé
- Estados: Manual/Auto
- Feedback visual inmediato
- Sincronización con ESP32

### SensorChart
- Gráficos de líneas con recharts
- Rangos: 1h, 6h, 24h, 7d
- Selector de sensores (temperatura, humedad, humedad suelo)
- Actualización automática

### RuleManager
- Crear/eliminar reglas de automatización
- Condiciones: sensor, operador, threshold
- Acciones: encender/apagar relé
- Estados: activa/inactiva

### LogViewer
- Visualización de logs del sistema
- Filtros por nivel (info, warning, error, debug)
- Filtros por fuente (esp32, api, system)
- Actualización automática cada 10s

## 🛠️ Desarrollo Local

Para trabajar en el dashboard localmente:

```bash
cd greenhouse-dashboard
npm install
npm run dev
```

El servidor de desarrollo iniciará en `http://localhost:5173`

### Variables de entorno

Crea un archivo `.env` para configurar la API:

```env
VITE_API_URL=http://168.181.185.42
```

## 🔄 Actualización del Dashboard

Para actualizar el dashboard:

```bash
# 1. Hacer cambios en el código
# 2. Crear nuevo build
npm run build

# 3. Ejecutar script de deployment
./deploy.sh

# 4. Seguir instrucciones para subir al VPS
```

## 📝 Notas

- El build de producción está optimizado (gzip: 253KB)
- El dashboard se conecta al API del VPS automáticamente
- No requiere configuración adicional después del deployment
- Los datos se actualizan en tiempo real desde el ESP32

## 🐛 Troubleshooting

### Dashboard no carga
```bash
# Verificar nginx
sudo systemctl status nginx
sudo nginx -t

# Verificar archivos
ls -la /opt/greenhouse-frontend/
```

### API no responde
```bash
# Verificar proceso Node.js
pm2 status
pm2 logs greenhouse-api

# Reiniciar API
pm2 restart greenhouse-api
```

### ESP32 no envía datos
```bash
# Verificar logs del API
pm2 logs greenhouse-api --lines 50

# Verificar MongoDB
mongo
> use greenhouse
> db.sensor_readings.find().sort({timestamp:-1}).limit(5)
```

## 🎉 Estado Actual

✅ **Backend VPS**: MongoDB + Node.js API funcionando en puerto 3000
✅ **Frontend**: Build de producción listo (250KB comprimido)
✅ **ESP32**: Enviando datos cada 30s, polling relés cada 5s
✅ **Comunicación**: Bidireccional ESP32 ↔ VPS funcionando
✅ **Dashboard**: Todos los componentes implementados y probados

**Listo para deployment!** 🚀
