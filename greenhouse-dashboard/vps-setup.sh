#!/bin/bash

# Script para configurar el VPS después de deployment
# Ejecutar DESDE el VPS (ssh -p 5591 root@168.181.185.42)

set -e

echo "🔧 Configurando VPS para Greenhouse Dashboard..."

# Aumentar rate limit para mejor experiencia
echo "📝 Ajustando rate limiter en API..."
sed -i 's/max: parseInt(process\.env\.RATE_LIMIT_MAX_REQUESTS) || 100/max: parseInt(process.env.RATE_LIMIT_MAX_REQUESTS) || 1000/' /opt/backend/server.js

# Reiniciar API
echo "🔄 Reiniciando API..."
pm2 restart greenhouse-api

# Verificar configuración de nginx
echo "✅ Verificando nginx..."
nginx -t

# Si nginx está bien, actualizar la configuración
cat > /etc/nginx/sites-available/greenhouse << 'EOF'
server {
    listen 80;
    server_name 168.181.185.42;

    # Frontend React
    location / {
        root /opt/greenhouse-frontend;
        index index.html;
        try_files $uri $uri/ /index.html;
        
        # Cache control para assets
        location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg)$ {
            expires 1y;
            add_header Cache-Control "public, immutable";
        }
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
        
        # CORS headers (importante para desarrollo)
        add_header 'Access-Control-Allow-Origin' '*' always;
        add_header 'Access-Control-Allow-Methods' 'GET, POST, PUT, DELETE, OPTIONS' always;
        add_header 'Access-Control-Allow-Headers' 'Content-Type' always;
    }

    # Health check
    location /health {
        proxy_pass http://localhost:3000;
        add_header 'Access-Control-Allow-Origin' '*' always;
    }
}
EOF

# Verificar nueva configuración
echo "✅ Verificando nueva configuración de nginx..."
nginx -t

# Reiniciar nginx
echo "🔄 Reiniciando nginx..."
systemctl reload nginx

echo ""
echo "✅ Configuración completada!"
echo ""
echo "📊 Estado de los servicios:"
echo ""
pm2 status
echo ""
systemctl status nginx --no-pager -l
echo ""
echo "🌐 Dashboard disponible en: http://168.181.185.42"
echo "🔌 API disponible en: http://168.181.185.42/api"
echo "💚 Health check: http://168.181.185.42/health"
