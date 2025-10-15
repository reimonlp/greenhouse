#!/bin/bash
# Deploy greenhouse backend to VPS

VPS_IP="168.181.185.42"
VPS_PORT="5591"
VPS_USER="root"

echo "╔════════════════════════════════════════════════╗"
echo "║  Greenhouse IoT - VPS Deployment Script       ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# Comprimir backend
echo "📦 Comprimiendo backend..."
cd "$(dirname "$0")"
tar czf backend.tar.gz backend/

echo "✓ Backend comprimido: backend.tar.gz"
echo ""

echo "📤 Para subir al VPS, ejecuta estos comandos:"
echo ""
echo "  scp -P ${VPS_PORT} backend.tar.gz ${VPS_USER}@${VPS_IP}:/opt/"
echo ""
echo "  ssh -p ${VPS_PORT} ${VPS_USER}@${VPS_IP}"
echo ""
echo "Luego en el VPS ejecuta:"
echo ""
echo "  cd /opt && tar xzf backend.tar.gz && cd backend"
echo "  npm install --production"
echo "  cp .env.example .env"
echo "  nano .env  # Editar si es necesario"
echo "  pm2 start server.js --name greenhouse-api"
echo "  pm2 save"
echo "  pm2 startup"
echo ""
echo "✓ Listo! Revisa el README.md para más detalles"
