# Actualización de Seguridad - Autenticación Bearer Token

## ⚠️ Cambios Importantes

Esta actualización agrega **autenticación obligatoria** para todas las comunicaciones ESP32 ↔️ Backend.

## 🔐 ¿Qué cambió?

### ESP32
- Envía token de autenticación en `device:register`
- Incluye header `Authorization: Bearer TOKEN` en peticiones HTTP
- Implementa backoff exponencial si falla la autenticación
- Rechaza operaciones sin autenticación exitosa

### Backend
- Valida tokens en WebSocket (`device:register`)
- Middleware HTTP para verificar Bearer tokens
- Desconecta clientes no autenticados
- Logs de intentos de autenticación

## 📋 Guía de Actualización

### 1. Generar Token Seguro

```bash
# Generar token aleatorio de 64 caracteres
openssl rand -hex 32
```

**Ejemplo de salida:**
```
9f8e7d6c5b4a3210fedcba9876543210abcdef1234567890abcdef1234567890
```

### 2. Configurar Backend

```bash
cd /home/reimon/greenhouse/backend-websocket-update

# Copiar y editar .env
cp .env.example .env
nano .env
```

**Editar ESP32_AUTH_TOKEN:**
```env
ESP32_AUTH_TOKEN=tu_token_generado_aqui
```

**Desplegar cambios:**
```bash
./deploy-auth.sh
```

### 3. Configurar ESP32

```bash
cd /home/reimon/greenhouse

# Editar secrets.h
nano include/secrets.h
```

**Agregar el mismo token:**
```cpp
#define DEVICE_AUTH_TOKEN "tu_token_generado_aqui"
```

**Compilar y subir:**
```bash
pio run --target upload
```

### 4. Verificar Funcionamiento

**Terminal 1 - Backend logs:**
```bash
pm2 logs greenhouse-backend | grep -i auth
```

**Terminal 2 - ESP32 serial:**
```bash
pio device monitor
```

**Deberías ver:**
```
Backend:
✓ Authentication successful for device: ESP32_GREENHOUSE_01

ESP32:
✓ Connected to server
✓ Authentication successful
✓ Device registered
```

## 🚨 Troubleshooting

### Error: "Authentication FAILED"

**ESP32 muestra:**
```
✗ Authentication FAILED - invalid token!
⚠ Retry after 30 seconds (attempt 1)
```

**Solución:**
1. Verifica que el token sea **exactamente igual** en:
   - `backend-websocket-update/.env` → `ESP32_AUTH_TOKEN`
   - `include/secrets.h` → `DEVICE_AUTH_TOKEN`
2. Sin espacios ni comillas extra
3. Reinicia backend: `pm2 restart greenhouse-backend`
4. Sube firmware actualizado al ESP32

### Error: HTTP 401/403

**Causa:** Token inválido en peticiones HTTP

**Solución:**
1. Verifica `.env` en backend
2. Recompila ESP32 con token correcto
3. Confirma que VPSClient usa `DEVICE_AUTH_TOKEN`

### Error: Backend no inicia

**Causa:** `.env` faltante

**Solución:**
```bash
cd /home/reimon/greenhouse/backend-websocket-update
cp .env.example .env
nano .env  # Configurar ESP32_AUTH_TOKEN
pm2 restart greenhouse-backend
```

## 📊 Monitoreo

### Ver intentos de autenticación
```bash
pm2 logs greenhouse-backend | grep "auth"
```

### Ver estado WebSocket
```bash
pm2 logs greenhouse-backend | grep "ESP32"
```

### Ver errores ESP32
```bash
pio device monitor --filter esp32_exception_decoder
```

## 🔄 Rollback (si algo falla)

### Backend
```bash
cd /opt/greenhouse-backend
git checkout server.js  # Versión anterior
pm2 restart greenhouse-backend
```

### ESP32
```bash
cd /home/reimon/greenhouse
git checkout include/vps_websocket.h src/vps_websocket.cpp src/vps_client.cpp
pio run --target upload
```

## 📝 Notas de Seguridad

### ✅ Buenas Prácticas
- Token mínimo de 32 caracteres hexadecimales (64 chars recomendado)
- Rotar token cada 3-6 meses
- Mantener `.env` y `secrets.h` fuera de Git
- Usar HTTPS/WSS siempre (ya configurado)

### ❌ Evitar
- Tokens simples como "123456" o "password"
- Reutilizar el mismo token en múltiples entornos
- Exponer tokens en logs o código público
- Commitear archivos con tokens reales

## 🔗 Documentación

Ver [SECURITY.md](/SECURITY.md) para detalles completos sobre:
- Arquitectura de seguridad
- Backoff exponencial
- Rotación de tokens
- Mejores prácticas

## 🆘 Soporte

Si encuentras problemas:

1. **Verificar logs:**
   ```bash
   pm2 logs greenhouse-backend --lines 50
   pio device monitor
   ```

2. **Revisar configuración:**
   ```bash
   cat backend-websocket-update/.env | grep ESP32_AUTH_TOKEN
   grep DEVICE_AUTH_TOKEN include/secrets.h
   ```

3. **Probar token manualmente:**
   ```bash
   curl -X POST http://localhost:3000/api/sensors \
     -H "Authorization: Bearer tu_token" \
     -H "Content-Type: application/json" \
     -d '{"temperature": 25}'
   ```

---

**Versión de seguridad:** 2.0  
**Fecha:** 2025-10-16  
**Breaking change:** Sí - requiere actualización de backend y firmware
