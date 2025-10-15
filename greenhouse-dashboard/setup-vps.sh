#!/bin/bash
# Script para configurar el dominio reimon.dev en el VPS
# Ejecutar en el VPS: bash setup-vps.sh

set -e

echo "🚀 Configurando reimon.dev/greenhouse en VPS..."
echo ""

# 1. Crear directorio para el frontend
echo "📁 Creando directorio /var/www/greenhouse..."
sudo mkdir -p /var/www/greenhouse
sudo chown -R www-data:www-data /var/www/greenhouse
echo "✅ Directorio creado"
echo ""

# 2. Verificar que nginx esté instalado
if ! command -v nginx &> /dev/null; then
    echo "⚠️  Nginx no está instalado. Instalando..."
    sudo apt update
    sudo apt install nginx -y
else
    echo "✅ Nginx está instalado"
fi
echo ""

# 3. Copiar configuración de nginx
echo "⚙️  Configurando Nginx..."
if [ -f "nginx-config-reimon.dev" ]; then
    sudo cp nginx-config-reimon.dev /etc/nginx/sites-available/reimon.dev
    echo "✅ Configuración copiada"
else
    echo "❌ Error: nginx-config-reimon.dev no encontrado"
    echo "   Copia el archivo manualmente o edita /etc/nginx/sites-available/reimon.dev"
fi
echo ""

# 4. Activar el sitio
echo "🔗 Activando sitio..."
sudo ln -sf /etc/nginx/sites-available/reimon.dev /etc/nginx/sites-enabled/
echo "✅ Sitio activado"
echo ""

# 5. Verificar configuración
echo "🔍 Verificando configuración de Nginx..."
sudo nginx -t
echo ""

# 6. Recargar nginx
echo "🔄 Recargando Nginx..."
sudo systemctl reload nginx
echo "✅ Nginx recargado"
echo ""

# 7. Verificar certbot
if ! command -v certbot &> /dev/null; then
    echo "⚠️  Certbot no está instalado. Instalando..."
    sudo apt update
    sudo apt install certbot python3-certbot-nginx -y
else
    echo "✅ Certbot está instalado"
fi
echo ""

# 8. Generar certificado SSL
echo "🔒 Configurando SSL con Let's Encrypt..."
echo "   Ejecuta manualmente:"
echo "   sudo certbot --nginx -d reimon.dev -d www.reimon.dev"
echo ""

# 9. Verificar API
echo "🔍 Verificando que la API esté corriendo..."
if curl -s http://localhost:80/api/health > /dev/null; then
    echo "✅ API respondiendo en puerto 80"
else
    echo "⚠️  API no responde. Verifica que esté corriendo:"
    echo "   pm2 list"
    echo "   pm2 logs greenhouse-backend"
fi
echo ""

echo "✅ Configuración base completada!"
echo ""
echo "📋 Próximos pasos:"
echo "   1. Configurar SSL: sudo certbot --nginx -d reimon.dev -d www.reimon.dev"
echo "   2. Desde tu máquina local, ejecuta: ./deploy-domain.sh"
echo "   3. Visita: https://reimon.dev/greenhouse"
