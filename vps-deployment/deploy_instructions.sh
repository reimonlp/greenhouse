#!/bin/bash
# Script para subir y configurar el backend en el VPS

VPS_HOST="168.181.185.42"
VPS_PORT="5591"
VPS_USER="root"
VPS_PASS="8nHVC)M()DN1ka"

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║     🚀 GREENHOUSE IoT - Deploy Backend to VPS                 ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Verificar que estamos en el directorio correcto
cd "$(dirname "$0")"

if [ ! -d "backend" ]; then
    echo "❌ Error: Directorio 'backend' no encontrado"
    exit 1
fi

# Comprimir backend
echo "📦 Comprimiendo backend..."
tar czf backend.tar.gz backend/
echo "   ✓ backend.tar.gz creado ($(du -h backend.tar.gz | cut -f1))"
echo ""

# Mostrar instrucciones
echo "📋 INSTRUCCIONES PARA COMPLETAR EL DEPLOY:"
echo ""
echo "1️⃣  Copia este comando para subir el instalador al VPS:"
echo ""
echo "   scp -P ${VPS_PORT} install_vps.sh ${VPS_USER}@${VPS_HOST}:/root/"
echo ""
echo "2️⃣  Conecta al VPS:"
echo ""
echo "   ssh -p ${VPS_PORT} ${VPS_USER}@${VPS_HOST}"
echo "   Password: ${VPS_PASS}"
echo ""
echo "3️⃣  En el VPS, ejecuta:"
echo ""
echo "   chmod +x /root/install_vps.sh"
echo "   /root/install_vps.sh"
echo ""
echo "4️⃣  Cuando termine, sube el backend:"
echo ""
echo "   exit  # Sal del VPS"
echo "   scp -P ${VPS_PORT} backend.tar.gz ${VPS_USER}@${VPS_HOST}:/opt/"
echo ""
echo "5️⃣  Vuelve a conectar al VPS e instala el backend:"
echo ""
echo "   ssh -p ${VPS_PORT} ${VPS_USER}@${VPS_HOST}"
echo "   cd /opt"
echo "   tar xzf backend.tar.gz"
echo "   cd backend"
echo "   npm install --production"
echo "   cp .env.example .env"
echo "   pm2 start server.js --name greenhouse-api"
echo "   pm2 save"
echo "   pm2 startup"
echo ""
echo "6️⃣  Verifica que funciona:"
echo ""
echo "   curl http://localhost:3000/health"
echo ""
echo "   # Debería responder: {\"status\":\"ok\",...,\"mongodb\":\"connected\"}"
echo ""
echo "7️⃣  Configura nginx:"
echo ""
cat << 'NGINX_CONFIG'
cat > /etc/nginx/sites-available/greenhouse << 'EOF'
server {
    listen 80;
    server_name 168.181.185.42;

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

    location /health {
        proxy_pass http://localhost:3000;
    }

    location / {
        root /opt/greenhouse-frontend;
        try_files $uri $uri/ /index.html;
    }
}
EOF

ln -sf /etc/nginx/sites-available/greenhouse /etc/nginx/sites-enabled/
rm -f /etc/nginx/sites-enabled/default
nginx -t
systemctl restart nginx
NGINX_CONFIG
echo ""
echo "8️⃣  Prueba desde tu navegador o terminal:"
echo ""
echo "   curl http://${VPS_HOST}/health"
echo ""
echo "   O abre en el navegador: http://${VPS_HOST}/health"
echo ""
echo "✅ ¡Listo para empezar!"
echo ""
