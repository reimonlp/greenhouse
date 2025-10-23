#!/bin/bash
# Script para generar token seguro y crear .env.production para Greenhouse
# Uso: ./generate_env.sh

ENV_PATH="/opt/greenhouse/.env.production"

# Generar token seguro
ESP32_AUTH_TOKEN="GHDEV_$(openssl rand -hex 16)"

# Configuración de producción
ALLOWED_ORIGINS="https://reimon.dev"
LOG_LEVEL="info"
RATE_LIMIT_WINDOW_MS="900000"
RATE_LIMIT_MAX_REQUESTS="5000"
PORT="3000"
NODE_ENV="production"
MONGODB_URI="mongodb://mongodb:27017/greenhouse"

cat <<EOF > "$ENV_PATH"
# Greenhouse Production Environment
ESP32_AUTH_TOKEN=$ESP32_AUTH_TOKEN
ALLOWED_ORIGINS=$ALLOWED_ORIGINS
LOG_LEVEL=$LOG_LEVEL
RATE_LIMIT_WINDOW_MS=$RATE_LIMIT_WINDOW_MS
RATE_LIMIT_MAX_REQUESTS=$RATE_LIMIT_MAX_REQUESTS
PORT=$PORT
NODE_ENV=$NODE_ENV
MONGODB_URI=$MONGODB_URI
EOF

echo "✅ Archivo .env.production generado en $ENV_PATH"
echo "🔑 Token para ESP32: $ESP32_AUTH_TOKEN"
echo "📝 Copia este token en esp32-firmware/include/secrets.h antes de compilar/subir el firmware."
echo ""
echo "🔧 Configuración:"
echo "   - Puerto: $PORT"
echo "   - Entorno: $NODE_ENV"
echo "   - MongoDB: $MONGODB_URI"
echo "   - Orígenes permitidos: $ALLOWED_ORIGINS"
