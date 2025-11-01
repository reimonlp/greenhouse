#!/bin/bash

# Script de prueba de autenticación
# Verifica que el sistema de tokens funciona correctamente

set -e

echo "=== Test de Autenticación ESP32 <-> Backend ==="

# Colores
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Configuración
BACKEND_URL="${BACKEND_URL:-https://reimon.dev/greenhouse}"
TOKEN="${ESP32_AUTH_TOKEN:-esp32_gh_prod_tk_9f8e7d6c5b4a3210fedcba9876543210abcdef1234567890}"

echo ""
echo "Backend URL: $BACKEND_URL"
echo "Token: ${TOKEN:0:20}...${TOKEN: -20}"
echo ""

# Test 1: Health check (sin autenticación)
echo -n "Test 1: Health check (público)... "
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" "$BACKEND_URL/health")
if [ "$RESPONSE" = "200" ]; then
    echo -e "${GREEN}✓ PASS${NC} (HTTP $RESPONSE)"
else
    echo -e "${RED}✗ FAIL${NC} (HTTP $RESPONSE)"
fi

# Test 2: POST sensor sin token (debe fallar)
echo -n "Test 2: POST sensor sin token... "
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" \
    -X POST "$BACKEND_URL/api/sensors" \
    -H "Content-Type: application/json" \
    -d '{"temperature": 25, "humidity": 60}')
if [ "$RESPONSE" = "401" ]; then
    echo -e "${GREEN}✓ PASS${NC} (HTTP $RESPONSE - rechazado correctamente)"
else
    echo -e "${RED}✗ FAIL${NC} (HTTP $RESPONSE - debería ser 401)"
fi

# Test 3: POST sensor con token inválido (debe fallar)
echo -n "Test 3: POST sensor token inválido... "
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" \
    -X POST "$BACKEND_URL/api/sensors" \
    -H "Authorization: Bearer token_invalido_12345" \
    -H "Content-Type: application/json" \
    -d '{"temperature": 25, "humidity": 60}')
if [ "$RESPONSE" = "403" ]; then
    echo -e "${GREEN}✓ PASS${NC} (HTTP $RESPONSE - rechazado correctamente)"
else
    echo -e "${RED}✗ FAIL${NC} (HTTP $RESPONSE - debería ser 403)"
fi

# Test 4: POST sensor con token válido (debe pasar)
echo -n "Test 4: POST sensor token válido... "
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" \
    -X POST "$BACKEND_URL/api/sensors" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"device_id":"ESP32_TEST","temperature":25.5,"humidity":60.2}')
if [ "$RESPONSE" = "201" ] || [ "$RESPONSE" = "200" ]; then
    echo -e "${GREEN}✓ PASS${NC} (HTTP $RESPONSE - autenticado correctamente)"
else
    echo -e "${RED}✗ FAIL${NC} (HTTP $RESPONSE - debería ser 200/201)"
    echo -e "${YELLOW}  Verifica que el token sea correcto en .env${NC}"
fi

# Test 5: GET sensors (público)
echo -n "Test 5: GET sensors (público)... "
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" "$BACKEND_URL/api/sensors")
if [ "$RESPONSE" = "200" ]; then
    echo -e "${GREEN}✓ PASS${NC} (HTTP $RESPONSE)"
else
    echo -e "${RED}✗ FAIL${NC} (HTTP $RESPONSE)"
fi

echo ""
echo -e "${GREEN}=== Tests completados ===${NC}"
echo ""
echo "Configuración actual:"
echo "  Backend: $BACKEND_URL"
echo "  Token ESP32 (primeros 20 chars): ${TOKEN:0:20}..."
echo ""
echo "Para ver logs del backend:"
echo "  pm2 logs greenhouse-backend | grep -i auth"
echo ""
