#!/bin/bash
# Script rápido de deploy completo - Ejecutar cuando tengas conexión estable
# Desde: /home/reimon/greenhouse

set -e

echo "🚀 DEPLOY COMPLETO DE WEBSOCKET"
echo "================================"
echo ""

# Paso 1: Subir archivos del backend al VPS
echo "📤 PASO 1: Subiendo archivos del backend al VPS..."
cd backend-websocket-update
scp -P 5591 server.js root@reimon.dev:/tmp/
scp -P 5591 nginx-config-websocket root@reimon.dev:/tmp/
scp -P 5591 update-websocket.sh root@reimon.dev:/tmp/
echo "✅ Archivos del backend subidos"
echo ""

# Paso 2: Ejecutar actualización en el VPS
echo "⚙️  PASO 2: Actualizando backend en el VPS..."
ssh -p 5591 root@reimon.dev 'cd /tmp && bash update-websocket.sh'
echo "✅ Backend actualizado"
echo ""

# Paso 3: Deploy del frontend
echo "🎨 PASO 3: Deploying frontend..."
cd ../greenhouse-dashboard
scp -P 5591 -r dist/* root@reimon.dev:/var/www/greenhouse/
echo "✅ Frontend desplegado"
echo ""

echo "🎉 DEPLOY COMPLETADO!"
echo ""
echo "✅ Verifica en: https://reimon.dev/greenhouse"
echo "   - Chip verde 'Conectado' debe aparecer en el header"
echo "   - Abre la consola: debe decir 'WebSocket conectado'"
echo ""
echo "📊 Monitoreo:"
echo "   ssh -p 5591 root@reimon.dev 'pm2 logs greenhouse-api'"
