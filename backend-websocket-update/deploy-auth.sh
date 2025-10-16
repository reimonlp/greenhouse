#!/bin/bash

# Script para actualizar el backend con autenticación
# Uso: ./deploy-auth.sh

set -e

echo "=== Actualizando Backend con Autenticación ==="

# Colores
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Directorio del script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# 1. Verificar que existe .env
if [ ! -f "$SCRIPT_DIR/.env" ]; then
    echo -e "${YELLOW}⚠ Archivo .env no encontrado${NC}"
    echo "Copiando desde .env.example..."
    cp "$SCRIPT_DIR/.env.example" "$SCRIPT_DIR/.env"
    echo -e "${RED}✗ IMPORTANTE: Edita .env y configura ESP32_AUTH_TOKEN!${NC}"
    echo "  nano $SCRIPT_DIR/.env"
    exit 1
fi

# 2. Verificar que ESP32_AUTH_TOKEN está configurado
if ! grep -q "ESP32_AUTH_TOKEN=" "$SCRIPT_DIR/.env"; then
    echo -e "${RED}✗ ESP32_AUTH_TOKEN no encontrado en .env${NC}"
    echo "Agrega: ESP32_AUTH_TOKEN=tu_token_aqui"
    exit 1
fi

# 3. Copiar server.js actualizado al servidor
echo -e "${GREEN}✓${NC} Copiando server.js actualizado..."
sudo cp "$SCRIPT_DIR/server.js" /opt/greenhouse-backend/server.js

# 4. Copiar .env (solo si no existe en producción)
if [ ! -f "/opt/greenhouse-backend/.env" ]; then
    echo -e "${GREEN}✓${NC} Copiando .env..."
    sudo cp "$SCRIPT_DIR/.env" /opt/greenhouse-backend/.env
else
    echo -e "${YELLOW}⚠${NC} .env ya existe en /opt/greenhouse-backend/"
    echo "  Verifica manualmente que ESP32_AUTH_TOKEN esté configurado"
fi

# 5. Reiniciar servicio
echo -e "${GREEN}✓${NC} Reiniciando backend..."
sudo pm2 restart greenhouse-backend

# 6. Verificar logs
echo ""
echo -e "${GREEN}=== Verificando logs ===${NC}"
sleep 2
sudo pm2 logs greenhouse-backend --lines 20 --nostream

echo ""
echo -e "${GREEN}✓ Backend actualizado con autenticación${NC}"
echo ""
echo -e "${YELLOW}PRÓXIMOS PASOS:${NC}"
echo "1. Verifica que el token en .env coincida con secrets.h del ESP32"
echo "2. Recompila y sube el firmware ESP32:"
echo "   cd /home/reimon/greenhouse"
echo "   pio run --target upload"
echo "3. Monitorea la autenticación:"
echo "   pm2 logs greenhouse-backend | grep -i auth"
echo ""
