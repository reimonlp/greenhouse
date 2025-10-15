#!/bin/bash

# Greenhouse Dashboard Deployment Script
# Este script despliega el frontend React en el VPS

set -e

echo "🚀 Iniciando deployment del Greenhouse Dashboard..."

# Variables
VPS_HOST="168.181.185.42"
VPS_PORT="5591"
VPS_USER="root"
FRONTEND_DIR="/opt/greenhouse-frontend"
BUILD_DIR="./dist"

# Colores para output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Verificar que existe el directorio dist
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}❌ Error: No existe el directorio dist. Ejecuta 'npm run build' primero.${NC}"
    exit 1
fi

echo -e "${YELLOW}📦 Comprimiendo archivos...${NC}"
cd dist
tar -czf ../greenhouse-frontend.tar.gz .
cd ..

echo -e "${GREEN}✓ Archivos comprimidos: greenhouse-frontend.tar.gz${NC}"
ls -lh greenhouse-frontend.tar.gz

echo ""
echo -e "${YELLOW}📤 Para subir al VPS, ejecuta:${NC}"
echo ""
echo "  scp -P $VPS_PORT greenhouse-frontend.tar.gz $VPS_USER@$VPS_HOST:/tmp/"
echo ""
echo -e "${YELLOW}📥 Luego conéctate al VPS y ejecuta:${NC}"
echo ""
echo "  ssh -p $VPS_PORT $VPS_USER@$VPS_HOST"
echo ""
echo "  # En el VPS:"
echo "  sudo mkdir -p $FRONTEND_DIR"
echo "  sudo tar -xzf /tmp/greenhouse-frontend.tar.gz -C $FRONTEND_DIR"
echo "  sudo chown -R www-data:www-data $FRONTEND_DIR"
echo "  sudo systemctl restart nginx"
echo ""
echo -e "${GREEN}✓ Script de deployment preparado${NC}"
echo -e "${YELLOW}💡 Tip: Asegúrate de tener configurado nginx para servir desde $FRONTEND_DIR${NC}"
