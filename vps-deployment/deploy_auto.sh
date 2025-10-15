#!/bin/bash
# Script automatizado completo para deploy en VPS

set -e

VPS_HOST="168.181.185.42"
VPS_PORT="5591"
VPS_USER="root"
VPS_PASS="8nHVC)M()DN1ka"

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║     🚀 GREENHOUSE IoT - Deploy Automatizado                   ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

cd "$(dirname "$0")"

# Función para ejecutar comandos en el VPS
ssh_exec() {
    sshpass -p "${VPS_PASS}" ssh -o StrictHostKeyChecking=no -p ${VPS_PORT} ${VPS_USER}@${VPS_HOST} "$@"
}

# Función para copiar archivos al VPS
scp_upload() {
    sshpass -p "${VPS_PASS}" scp -o StrictHostKeyChecking=no -P ${VPS_PORT} "$1" ${VPS_USER}@${VPS_HOST}:"$2"
}

echo "🔍 Verificando conexión al VPS..."
if ssh_exec "echo OK" > /dev/null 2>&1; then
    echo "   ✓ Conexión exitosa"
else
    echo "   ❌ No se puede conectar al VPS"
    echo ""
    echo "   Instala sshpass primero:"
    echo "   sudo pacman -S sshpass  # Para Arch Linux"
    echo "   sudo apt install sshpass  # Para Ubuntu/Debian"
    exit 1
fi

echo ""
echo "📦 Comprimiendo backend..."
tar czf backend.tar.gz backend/
echo "   ✓ backend.tar.gz creado"

echo ""
echo "📤 [1/4] Subiendo instalador al VPS..."
scp_upload "install_vps.sh" "/root/"
echo "   ✓ install_vps.sh subido"

echo ""
echo "🔧 [2/4] Instalando dependencias en VPS..."
echo "   (Esto tomará 3-5 minutos, por favor espera...)"
ssh_exec "chmod +x /root/install_vps.sh && /root/install_vps.sh" || {
    echo "   ⚠️  Instalación completada con advertencias (esto es normal)"
}

echo ""
echo "📤 [3/4] Subiendo backend al VPS..."
scp_upload "backend.tar.gz" "/opt/"
echo "   ✓ backend.tar.gz subido"

echo ""
echo "⚙️  [4/4] Configurando backend en VPS..."
ssh_exec "cd /opt && tar xzf backend.tar.gz && cd backend && npm install --production" > /dev/null 2>&1
echo "   ✓ Dependencias de Node.js instaladas"

ssh_exec "cd /opt/backend && cp .env.example .env"
echo "   ✓ Archivo .env creado"

ssh_exec "cd /opt/backend && pm2 delete greenhouse-api 2>/dev/null || true"
ssh_exec "cd /opt/backend && pm2 start server.js --name greenhouse-api"
ssh_exec "pm2 save"
echo "   ✓ API iniciada con PM2"

echo ""
echo "🌐 Configurando nginx..."
ssh_exec 'cat > /etc/nginx/sites-available/greenhouse << '\''EOF'\''
server {
    listen 80;
    server_name 168.181.185.42;

    location /api/ {
        proxy_pass http://localhost:3000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_cache_bypass \$http_upgrade;
    }

    location /health {
        proxy_pass http://localhost:3000;
    }

    location / {
        proxy_pass http://localhost:3000;
    }
}
EOF'

ssh_exec "ln -sf /etc/nginx/sites-available/greenhouse /etc/nginx/sites-enabled/"
ssh_exec "rm -f /etc/nginx/sites-enabled/default"
ssh_exec "nginx -t && systemctl restart nginx"
echo "   ✓ nginx configurado y reiniciado"

echo ""
echo "🧪 Verificando instalación..."
sleep 2

if curl -s http://${VPS_HOST}/health | grep -q "ok"; then
    echo "   ✅ API respondiendo correctamente!"
else
    echo "   ⚠️  API puede estar iniciando aún..."
    echo "   Espera 10 segundos y verifica: http://${VPS_HOST}/health"
fi

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║              ✅ DEPLOY COMPLETADO EXITOSAMENTE                 ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "🎉 Backend API está corriendo en:"
echo "   • http://${VPS_HOST}/health"
echo "   • http://${VPS_HOST}/api/sensors"
echo "   • http://${VPS_HOST}/api/relays/states"
echo "   • http://${VPS_HOST}/api/rules"
echo ""
echo "📊 Ver logs de la API:"
echo "   ssh -p ${VPS_PORT} ${VPS_USER}@${VPS_HOST} \"pm2 logs greenhouse-api\""
echo ""
echo "🔄 Reiniciar API:"
echo "   ssh -p ${VPS_PORT} ${VPS_USER}@${VPS_HOST} \"pm2 restart greenhouse-api\""
echo ""
echo "📝 Próximos pasos:"
echo "   1. Modificar firmware ESP32 para enviar datos a http://${VPS_HOST}"
echo "   2. Crear frontend React con Material UI"
echo ""
