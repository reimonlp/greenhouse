# Seguridad del Sistema Greenhouse ESP32

## Autenticación Implementada

### Token Bearer para ESP32

El sistema ahora requiere un token de autenticación para todas las comunicaciones entre el ESP32 y el servidor VPS.

## Configuración

### 1. ESP32 (Cliente)

**Archivo: `include/secrets.h`**
```cpp
#define DEVICE_AUTH_TOKEN "esp32_gh_prod_tk_9f8e7d6c5b4a3210fedcba9876543210abcdef1234567890"
```

⚠️ **IMPORTANTE**: Cambia este token en producción!

**Generar un token seguro:**
```bash
# Linux/Mac
openssl rand -hex 32

# O usa un generador online (ej: random.org)
```

### 2. Backend (Servidor)

**Archivo: `backend-websocket-update/.env`**
```env
ESP32_AUTH_TOKEN=esp32_gh_prod_tk_9f8e7d6c5b4a3210fedcba9876543210abcdef1234567890
```

**Usa el mismo token que en el ESP32!**

## Cómo Funciona

### WebSocket Authentication

1. **ESP32 se conecta al servidor WebSocket**
2. **Envía evento `device:register` con el token:**
   ```json
   {
     "device_id": "ESP32_GREENHOUSE_01",
     "device_type": "esp32",
     "firmware_version": "2.1-websocket",
     "auth_token": "tu_token_aqui"
   }
   ```

3. **Servidor valida el token:**
   - ✅ Si es válido → emite `device:auth_success`
   - ❌ Si es inválido → emite `device:auth_failed` y desconecta

4. **ESP32 maneja la respuesta:**
   - ✅ Success: opera normalmente
   - ❌ Failure: backoff exponencial (30s, 60s, 120s, 240s, 300s max)

### HTTP API Authentication

Todas las peticiones HTTP deben incluir el header:
```
Authorization: Bearer tu_token_aqui
```

**Ejemplo con curl:**
```bash
curl -X POST https://reimon.dev/greenhouse/api/sensors \
  -H "Authorization: Bearer esp32_gh_prod_tk_..." \
  -H "Content-Type: application/json" \
  -d '{"temperature": 25.5, "humidity": 60}'
```

**Respuestas de error:**
- `401 Unauthorized`: Token faltante o formato incorrecto
- `403 Forbidden`: Token inválido

## Seguridad Mejorada

### Protecciones Implementadas

1. **✅ Autenticación obligatoria**
   - WebSocket: validación en `device:register`
   - HTTP API: middleware `authenticateESP32`

2. **✅ Desconexión automática**
   - Clientes no autenticados son desconectados
   - Eventos rechazados sin autenticación válida

3. **✅ Backoff exponencial**
   - Evita ataques de fuerza bruta
   - Delay crece: 30s → 60s → 120s → 240s → 300s
   - Mensaje de advertencia después de 5 intentos fallidos

4. **✅ Logs de seguridad**
   - Intentos de autenticación registrados
   - Eventos no autorizados loggeados

### Eventos Protegidos

**WebSocket (solo dispositivos autenticados):**
- `sensor:data` - envío de lecturas de sensores
- `relay:state` - actualización de estados de relays
- `log` - envío de logs del sistema

**HTTP API (requiere Bearer token):**
- `POST /api/sensors` - crear lectura de sensor
- Otras rutas protegidas según sea necesario

## Mejores Prácticas

### ✅ DO (Hacer)

1. **Cambiar el token por defecto inmediatamente**
2. **Usar tokens largos y aleatorios** (mínimo 32 caracteres)
3. **No compartir tokens en código público**
4. **Usar variables de entorno en producción**
5. **Rotar tokens periódicamente** (cada 3-6 meses)
6. **Mantener `.env` fuera del control de versiones**

### ❌ DON'T (No hacer)

1. ❌ No uses tokens simples como "123456" o "password"
2. ❌ No comitees `secrets.h` con tokens reales
3. ❌ No reutilices el mismo token para múltiples dispositivos
4. ❌ No expongas el token en logs públicos
5. ❌ No uses HTTP sin TLS (siempre HTTPS/WSS)

## Rotación de Tokens

Para cambiar el token:

1. **Generar nuevo token:**
   ```bash
   openssl rand -hex 32
   ```

2. **Actualizar backend:**
   ```bash
   cd backend-websocket-update
   nano .env  # Actualizar ESP32_AUTH_TOKEN
   pm2 restart greenhouse-backend
   ```

3. **Actualizar ESP32:**
   ```cpp
   // include/secrets.h
   #define DEVICE_AUTH_TOKEN "nuevo_token_aqui"
   ```

4. **Recompilar y subir firmware:**
   ```bash
   pio run --target upload
   ```

## Monitoreo

### Verificar autenticación exitosa

**Backend logs:**
```bash
pm2 logs greenhouse-backend | grep -i auth
```

Deberías ver:
```
✓ Authentication successful for device: ESP32_GREENHOUSE_01
```

**ESP32 Serial Monitor:**
```
✓ Authentication successful
✓ Device registered
```

### Detectar intentos fallidos

**Backend:**
```
✗ Authentication failed for device: ESP32_GREENHOUSE_01
```

**ESP32:**
```
✗ Authentication FAILED - invalid token!
⚠ Retry after 30 seconds (attempt 1)
```

## Troubleshooting

### Problema: ESP32 no se conecta

**Síntomas:**
```
✗ Authentication FAILED - invalid token!
⚠ Retry after 30 seconds
```

**Solución:**
1. Verificar que tokens coinciden:
   - `include/secrets.h` en ESP32
   - `.env` en backend
2. Reiniciar backend: `pm2 restart greenhouse-backend`
3. Reiniciar ESP32

### Problema: HTTP 401/403

**Síntomas:**
```
Response code: 401
✗ Authentication failed - invalid token!
```

**Solución:**
1. Verificar header `Authorization: Bearer ...`
2. Confirmar token correcto en `.env`
3. Verificar que backend está usando el token actualizado

## Arquitectura de Seguridad

```
┌─────────────┐                      ┌──────────────┐
│   ESP32     │   Auth Required      │   Backend    │
│             │─────────────────────▶│  (Node.js)   │
│  secrets.h  │  Bearer Token        │    .env      │
└─────────────┘                      └──────────────┘
      │                                      │
      │  1. device:register                 │
      │     + auth_token                    │
      ├─────────────────────────────────────▶
      │                                      │
      │  2. Validate token                  │
      │                                      ├─ Check ESP32_AUTH_TOKEN
      │                                      │
      │  3a. device:auth_success            │
      │◀─────────────────────────────────────┤  (if valid)
      │     OR                               │
      │  3b. device:auth_failed             │
      │◀─────────────────────────────────────┤  (if invalid)
      │     + disconnect                     │
      │                                      │
      │  4. Exponential backoff (if failed) │
      │     30s → 60s → 120s → 240s → 300s  │
      │                                      │
```

## Cumplimiento

- ✅ **Autenticación**: Todos los dispositivos deben autenticarse
- ✅ **Autorización**: Solo dispositivos autenticados pueden enviar comandos
- ✅ **Confidencialidad**: Comunicación sobre TLS (HTTPS/WSS)
- ✅ **Integridad**: Tokens verificados en cada conexión
- ✅ **Rate Limiting**: Backoff exponencial previene ataques

---

**Versión:** 2.0  
**Última actualización:** 2025-10-16  
**Autor:** Sistema Greenhouse ESP32
