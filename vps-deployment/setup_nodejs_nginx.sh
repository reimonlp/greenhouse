#!/bin/bash
# Setup script for Node.js and nginx on Ubuntu 24.04
set -e

echo "=== Instalando Node.js 20 LTS ==="
curl -fsSL https://deb.nodesource.com/setup_20.x | bash -
apt-get install -y nodejs

echo "=== Instalando nginx ==="
apt-get install -y nginx

echo "=== Instalando PM2 para gestionar procesos Node.js ==="
npm install -g pm2

echo "=== Creando directorio para la aplicación ==="
mkdir -p /opt/greenhouse-api
mkdir -p /opt/greenhouse-frontend

echo "=== Configurando firewall (ufw) ==="
ufw allow 5591/tcp  # SSH
ufw allow 80/tcp    # HTTP
ufw allow 443/tcp   # HTTPS
ufw --force enable

echo "=== Versiones instaladas ==="
node --version
npm --version
nginx -v
pm2 --version

echo "=== Node.js, nginx y PM2 configurados exitosamente ==="
