# 🌐 Configuración de Dominio reimon.dev

## 📋 Objetivo
Configurar el dominio `reimon.dev` para acceder al sistema greenhouse:
- Frontend: `https://reimon.dev/greenhouse`
- API: `https://reimon.dev/greenhouse/api`

---

## 🔧 Configuración Requerida

### 1️⃣ **DNS (Ya configurado si reimon.dev apunta a tu VPS)**

Verifica que el dominio apunte a tu VPS:
```bash
dig reimon.dev
# Debe mostrar: 168.181.185.42
```

---

### 2️⃣ **Nginx en VPS (Reverse Proxy)**

Crea el archivo de configuración:

```bash
sudo nano /etc/nginx/sites-available/reimon.dev
```

**Contenido del archivo:**

```nginx
server {
    listen 80;
    listen [::]:80;
    server_name reimon.dev www.reimon.dev;

    # Redirigir HTTP a HTTPS (después de configurar SSL)
    return 301 https://$server_name$request_uri;
}

server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name reimon.dev www.reimon.dev;

    # Certificados SSL (se generan con certbot)
    ssl_certificate /etc/letsencrypt/live/reimon.dev/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/reimon.dev/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;

    # Logs
    access_log /var/log/nginx/reimon.dev.access.log;
    error_log /var/log/nginx/reimon.dev.error.log;

    # API Backend (Node.js en puerto 80 del VPS)
    location /greenhouse/api/ {
        proxy_pass http://localhost:80/api/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_cache_bypass $http_upgrade;
        
        # CORS headers
        add_header 'Access-Control-Allow-Origin' '*' always;
        add_header 'Access-Control-Allow-Methods' 'GET, POST, PUT, DELETE, OPTIONS' always;
        add_header 'Access-Control-Allow-Headers' 'Content-Type, Authorization' always;
    }

    # Frontend (React build en /var/www/greenhouse)
    location /greenhouse/ {
        alias /var/www/greenhouse/;
        try_files $uri $uri/ /greenhouse/index.html;
        
        # Cache para assets estáticos
        location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg|woff|woff2|ttf|eot)$ {
            expires 1y;
            add_header Cache-Control "public, immutable";
        }
    }

    # Opcional: Redirigir raíz a /greenhouse
    location = / {
        return 301 https://reimon.dev/greenhouse;
    }
}
```

**Activar el sitio:**

```bash
# Crear enlace simbólico
sudo ln -s /etc/nginx/sites-available/reimon.dev /etc/nginx/sites-enabled/

# Verificar configuración
sudo nginx -t

# Recargar nginx
sudo systemctl reload nginx
```

---

### 3️⃣ **Certificado SSL con Let's Encrypt**

```bash
# Instalar certbot
sudo apt update
sudo apt install certbot python3-certbot-nginx -y

# Generar certificado
sudo certbot --nginx -d reimon.dev -d www.reimon.dev

# Verificar renovación automática
sudo certbot renew --dry-run
```

---

### 4️⃣ **Actualizar Frontend - Vite Configuration**

Edita `greenhouse-dashboard/vite.config.js`:

```javascript
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  base: '/greenhouse/', // ⚠️ IMPORTANTE: Base path
  server: {
    port: 5173,
    host: true
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets'
  }
})
```

---

### 5️⃣ **Actualizar Frontend - API URL**

Crea `greenhouse-dashboard/.env.production`:

```bash
# API URL para producción
VITE_API_URL=https://reimon.dev/greenhouse/api
```

---

### 6️⃣ **Rebuild y Deploy Frontend**

```bash
cd greenhouse-dashboard

# Instalar dependencias
npm install

# Build para producción
npm run build

# Copiar al VPS
scp -r dist/* root@168.181.185.42:/var/www/greenhouse/

# O con rsync
rsync -avz --delete dist/ root@168.181.185.42:/var/www/greenhouse/
```

---

### 7️⃣ **Crear Script de Deploy Automático**

Crea `greenhouse-dashboard/deploy-domain.sh`:

```bash
#!/bin/bash
# Deploy script para reimon.dev/greenhouse

set -e

echo "🔨 Building frontend..."
npm run build

echo "📦 Deploying to VPS..."
rsync -avz --delete \
  dist/ \
  root@168.181.185.42:/var/www/greenhouse/

echo "✅ Deploy completed!"
echo "🌐 Visit: https://reimon.dev/greenhouse"
```

```bash
chmod +x greenhouse-dashboard/deploy-domain.sh
```

---

## 🚀 Pasos de Implementación

### **En el VPS (168.181.185.42):**

```bash
# 1. Crear directorio para el frontend
sudo mkdir -p /var/www/greenhouse
sudo chown -R www-data:www-data /var/www/greenhouse

# 2. Crear configuración nginx
sudo nano /etc/nginx/sites-available/reimon.dev
# (Copiar la configuración de arriba)

# 3. Activar sitio
sudo ln -s /etc/nginx/sites-available/reimon.dev /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx

# 4. Configurar SSL
sudo certbot --nginx -d reimon.dev -d www.reimon.dev

# 5. Verificar que la API esté corriendo en puerto 80
curl http://localhost:80/api/health
```

### **En tu máquina local:**

```bash
cd /home/reimon/greenhouse/greenhouse-dashboard

# 1. Actualizar vite.config.js (base: '/greenhouse/')

# 2. Crear .env.production
echo "VITE_API_URL=https://reimon.dev/greenhouse/api" > .env.production

# 3. Build y deploy
npm install
npm run build
rsync -avz --delete dist/ root@168.181.185.42:/var/www/greenhouse/
```

---

## ✅ Verificación

Después de la configuración, verifica:

### **Frontend:**
```bash
curl -I https://reimon.dev/greenhouse
# Debe retornar: 200 OK
```

### **API:**
```bash
curl https://reimon.dev/greenhouse/api/health
# Debe retornar: JSON con status del sistema
```

### **En el navegador:**
- Abre: `https://reimon.dev/greenhouse`
- Verifica que cargue el dashboard
- Verifica que los datos se cargan correctamente desde la API

---

## 🔍 Troubleshooting

### **Error 404 en rutas del frontend:**
```bash
# Verificar que try_files esté correcto en nginx
location /greenhouse/ {
    alias /var/www/greenhouse/;
    try_files $uri $uri/ /greenhouse/index.html;
}
```

### **Error de CORS en API:**
```bash
# Verificar headers en nginx
add_header 'Access-Control-Allow-Origin' '*' always;
```

### **Assets no cargan (CSS/JS):**
```bash
# Verificar base path en vite.config.js
base: '/greenhouse/'
```

### **API no responde:**
```bash
# Verificar que el backend esté corriendo
ssh root@168.181.185.42
pm2 list
pm2 logs greenhouse-backend
```

---

## 📁 Estructura Final

```
https://reimon.dev/
├── /                          → Redirect a /greenhouse
├── /greenhouse/               → Frontend React
│   ├── /assets/              → CSS, JS, imágenes
│   └── index.html            → App principal
└── /greenhouse/api/          → API Backend
    ├── /sensors              → GET sensor data
    ├── /relays               → GET/POST relay control
    ├── /health               → System health
    └── ...                   → Otros endpoints
```

---

## 🔒 Seguridad

- ✅ HTTPS obligatorio (SSL con Let's Encrypt)
- ✅ Headers de seguridad configurados
- ✅ CORS correctamente configurado
- ✅ Logs habilitados para auditoría

---

## 📝 Notas

1. **Certificado SSL**: Se renueva automáticamente cada 90 días con certbot
2. **Cache**: Assets estáticos se cachean por 1 año
3. **Backups**: Considera hacer backup de `/var/www/greenhouse` regularmente
4. **Monitoreo**: Usa `pm2 logs` para ver logs del backend

---

## 🎯 Siguiente Paso

Ejecuta estos comandos para implementar:

```bash
# En el VPS
ssh root@168.181.185.42
# Sigue los pasos de "En el VPS" de arriba

# En tu máquina
cd /home/reimon/greenhouse/greenhouse-dashboard
# Sigue los pasos de "En tu máquina local"
```
