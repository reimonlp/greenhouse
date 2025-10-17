#!/bin/bash
set -e

# Instalar PM2 globalmente
npm install -g pm2

# Backend: instalar dependencias y lanzar con PM2
cd /opt/greenhouse/backend-websocket-update
npm ci --omit=dev
pm2 start server.js --name greenhouse-backend
pm2 save
pm2 startup systemd

# Frontend: instalar dependencias, compilar y copiar
cd /opt/greenhouse/greenhouse-dashboard
npm ci --omit=dev
npm run build
rm -rf node_modules .vite
rm -rf /var/www/greenhouse
mkdir -p /var/www/greenhouse
cp -r dist/* /var/www/greenhouse/
chown -R www-data:www-data /var/www/greenhouse

# Limpiar archivos innecesarios
find /opt/greenhouse -name "node_modules" -type d -exec rm -rf {} +
find /opt/greenhouse -name ".cache" -type d -exec rm -rf {} +
find /opt/greenhouse -name "*.log" -type f -delete

# Recargar nginx
systemctl reload nginx

echo "✅ Greenhouse backend y frontend desplegados y limpios con PM2 y nginx."