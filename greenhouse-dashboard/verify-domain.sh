#!/bin/bash
# Script de verificación post-deploy

echo "🔍 Verificando configuración de reimon.dev/greenhouse..."
echo ""

# 1. Verificar frontend
echo "1️⃣ Frontend (https://reimon.dev/greenhouse):"
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" https://reimon.dev/greenhouse 2>/dev/null || echo "000")
if [ "$HTTP_CODE" = "200" ]; then
    echo "   ✅ Frontend respondiendo correctamente (HTTP $HTTP_CODE)"
else
    echo "   ❌ Frontend no responde (HTTP $HTTP_CODE)"
fi
echo ""

# 2. Verificar API
echo "2️⃣ API (https://reimon.dev/greenhouse/api/health):"
API_RESPONSE=$(curl -s https://reimon.dev/greenhouse/api/health 2>/dev/null || echo "ERROR")
if [ "$API_RESPONSE" != "ERROR" ]; then
    echo "   ✅ API respondiendo"
    echo "   Respuesta: $API_RESPONSE"
else
    echo "   ❌ API no responde"
fi
echo ""

# 3. Verificar SSL
echo "3️⃣ Certificado SSL:"
SSL_INFO=$(echo | openssl s_client -connect reimon.dev:443 -servername reimon.dev 2>/dev/null | grep "subject=" || echo "ERROR")
if [ "$SSL_INFO" != "ERROR" ]; then
    echo "   ✅ SSL configurado correctamente"
    echo "   $SSL_INFO"
else
    echo "   ⚠️  No se pudo verificar SSL"
fi
echo ""

# 4. Verificar DNS
echo "4️⃣ DNS (reimon.dev → 168.181.185.42):"
IP=$(dig +short reimon.dev | tail -n1)
if [ "$IP" = "168.181.185.42" ]; then
    echo "   ✅ DNS apuntando correctamente"
else
    echo "   ⚠️  DNS apunta a: $IP (esperado: 168.181.185.42)"
fi
echo ""

echo "✅ Verificación completada"
