#!/bin/bash
# Script de actualización para WebSocket
# Ejecutar en el VPS con: bash update-websocket.sh

set -e

echo "🚀 Actualizando sistema Greenhouse a WebSocket..."
echo ""

# 1. Verificar que socket.io está instalado
echo "1️⃣ Verificando dependencias..."
cd /opt/backend
if grep -q "socket.io" package.json; then
    echo "   ✓ socket.io ya instalado"
else
    echo "   ⚙️  Instalando socket.io..."
    npm install socket.io
fi
echo ""

# 2. Backup del server.js actual
echo "2️⃣ Creando backup..."
cp server.js server.js.backup-$(date +%Y%m%d-%H%M%S)
echo "   ✓ Backup creado"
echo ""

# 3. Copiar nuevo server.js
echo "3️⃣ Actualizando server.js..."
if [ -f "/tmp/server.js" ]; then
    cp /tmp/server.js server.js
    echo "   ✓ server.js actualizado"
else
    echo "   ⚠️  Archivo /tmp/server.js no encontrado"
    echo "   Súbelo primero con: scp -P 5591 server.js root@168.181.185.42:/tmp/"
    exit 1
fi
echo ""

# 4. Actualizar configuración de Nginx
echo "4️⃣ Actualizando Nginx..."
if [ -f "/tmp/nginx-config-websocket" ]; then
    cp /tmp/nginx-config-websocket /etc/nginx/sites-available/reimon.dev
    nginx -t
    systemctl reload nginx
    echo "   ✓ Nginx actualizado y recargado"
else
    echo "   ⚠️  Archivo /tmp/nginx-config-websocket no encontrado"
    exit 1
fi
echo ""

# 5. Reiniciar backend
echo "5️⃣ Reiniciando backend con PM2..."
pm2 restart greenhouse-api
pm2 save
echo "   ✓ Backend reiniciado"
echo ""

# 6. Verificar logs
echo "6️⃣ Verificando logs (últimas 10 líneas)..."
sleep 2
pm2 logs greenhouse-api --lines 10 --nostream
echo ""

echo "✅ Actualización completada!"
echo ""
echo "📋 Verificación:"
echo "   - WebSocket: wss://reimon.dev/greenhouse/socket.io/"
echo "   - API: https://reimon.dev/greenhouse/api/"
echo "   - Frontend: https://reimon.dev/greenhouse/"
echo ""
echo "💡 Comandos útiles:"
echo "   pm2 logs greenhouse-api           # Ver logs en tiempo real"
echo "   pm2 restart greenhouse-api        # Reiniciar backend"
echo "   nginx -t && systemctl reload nginx # Recargar Nginx"
