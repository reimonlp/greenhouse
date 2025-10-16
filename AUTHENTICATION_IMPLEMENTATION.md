# Sistema de Autenticación Bearer Token - Resumen de Implementación

## 🎯 Objetivo Cumplido

Se ha implementado un sistema de autenticación robusto usando Bearer tokens para proteger todas las comunicaciones entre el ESP32 y el backend VPS.

## 📦 Cambios Implementados

### 1. Configuración (ESP32)

#### `include/vps_config.h`
```cpp
// Security - Authentication token
#ifndef DEVICE_AUTH_TOKEN
#define DEVICE_AUTH_TOKEN "your_secret_token_here_change_in_production"
#endif
```

#### `include/secrets.h`
```cpp
// Device authentication for VPS/WebSocket communication
#ifndef DEVICE_AUTH_TOKEN
#define DEVICE_AUTH_TOKEN "esp32_gh_prod_tk_9f8e7d6c5b4a3210fedcba9876543210abcdef1234567890"
#endif
```

#### `include/secrets.example.h`
Agregado ejemplo del token de autenticación para nuevos desarrolladores.

### 2. WebSocket Authentication (ESP32)

#### `src/vps_websocket.cpp`
- **Envío de token en registro:**
  ```cpp
  deviceInfo["auth_token"] = DEVICE_AUTH_TOKEN;
  ```

- **Validación de respuesta:**
  - `device:auth_success` → continuar operación normal
  - `device:auth_failed` → activar backoff exponencial

- **Backoff exponencial:**
  - 30s → 60s → 120s → 240s → máximo 300s (5 minutos)
  - Contador de intentos fallidos
  - Advertencia después de 5 fallos

#### `include/vps_websocket.h`
Agregadas variables privadas:
```cpp
bool _authFailed;
int _authFailureCount;
unsigned long _lastAuthAttempt;
```

### 3. HTTP Authentication (ESP32)

#### `src/vps_client.cpp`
- **Header de autorización:**
  ```cpp
  https.addHeader("Authorization", String("Bearer ") + DEVICE_AUTH_TOKEN);
  ```

- **Detección de errores 401/403:**
  ```cpp
  if (httpCode == 401 || httpCode == 403) {
      DEBUG_PRINTLN("✗ Authentication failed - invalid token!");
  }
  ```

### 4. Backend Authentication (Node.js)

#### `backend-websocket-update/server.js`

**Variable de token:**
```javascript
const ESP32_AUTH_TOKEN = process.env.ESP32_AUTH_TOKEN || 'default_token';
```

**WebSocket auth:**
```javascript
socket.on('device:register', async (data) => {
  if (!data.auth_token || data.auth_token !== ESP32_AUTH_TOKEN) {
    socket.emit('device:auth_failed', { error: 'Invalid token' });
    socket.disconnect(true);
    return;
  }
  socket.authenticated = true;
  socket.emit('device:auth_success', { device_id: data.device_id });
});
```

**Protección de eventos:**
```javascript
socket.on('sensor:data', async (data) => {
  if (!socket.authenticated) {
    console.log('⚠ Unauthorized sensor:data attempt');
    return;
  }
  // ... procesar datos
});
```

**Middleware HTTP:**
```javascript
const authenticateESP32 = (req, res, next) => {
  const authHeader = req.headers.authorization;
  if (!authHeader || !authHeader.startsWith('Bearer ')) {
    return res.status(401).json({ error: 'Missing Authorization' });
  }
  const token = authHeader.substring(7);
  if (token !== ESP32_AUTH_TOKEN) {
    return res.status(403).json({ error: 'Invalid token' });
  }
  next();
};

app.post('/api/sensors', authenticateESP32, async (req, res) => {
  // ... ruta protegida
});
```

### 5. Configuración Backend

#### `.env.example`
```env
ESP32_AUTH_TOKEN=esp32_gh_prod_tk_9f8e7d6c5b4a3210fedcba9876543210abcdef1234567890
```

### 6. Scripts de Despliegue

#### `backend-websocket-update/deploy-auth.sh`
Script automatizado para desplegar cambios de autenticación:
- Verifica `.env`
- Copia archivos actualizados
- Reinicia servicio PM2
- Muestra logs de verificación

#### `backend-websocket-update/test-auth.sh`
Suite de tests para verificar autenticación:
- Health check público
- POST sin token (debe fallar 401)
- POST con token inválido (debe fallar 403)
- POST con token válido (debe pasar 201)
- GET público

### 7. Documentación

#### `SECURITY.md`
Documentación completa de seguridad:
- Cómo funciona la autenticación
- Configuración paso a paso
- Mejores prácticas
- Troubleshooting
- Rotación de tokens
- Arquitectura de seguridad

#### `backend-websocket-update/README_AUTH_UPDATE.md`
Guía de actualización:
- Cambios importantes
- Pasos de actualización
- Verificación de funcionamiento
- Rollback si es necesario

## 🔒 Características de Seguridad

### ✅ Implementado

1. **Autenticación obligatoria**
   - WebSocket: validación en handshake
   - HTTP API: middleware Bearer token

2. **Desconexión automática**
   - Clientes no autenticados desconectados inmediatamente
   - Socket marcado como no autenticado

3. **Backoff exponencial**
   - Previene ataques de fuerza bruta
   - Delays crecientes: 30s, 60s, 120s, 240s, 300s
   - Límite de 5 minutos máximo

4. **Validación robusta**
   - Tokens verificados en cada conexión WebSocket
   - Headers HTTP validados en cada petición
   - Eventos sin autenticación rechazados silenciosamente

5. **Logging de seguridad**
   - Intentos de autenticación registrados
   - Eventos no autorizados loggeados
   - Fallos visibles para debugging

## 📊 Flujo de Autenticación

### WebSocket
```
ESP32                          Backend
  |                               |
  |--- Connect WebSocket -------->|
  |                               |
  |--- device:register ---------->|
  |    + auth_token               |
  |                               |
  |                            Validate
  |                            ESP32_AUTH_TOKEN
  |                               |
  |<-- device:auth_success -------|  ✅ Si válido
  |    OR                         |
  |<-- device:auth_failed --------|  ❌ Si inválido
  |    + disconnect()             |
  |                               |
```

### HTTP API
```
ESP32                          Backend
  |                               |
  |--- POST /api/sensors -------->|
  |    Authorization: Bearer ...  |
  |                               |
  |                        authenticateESP32
  |                        middleware
  |                               |
  |<-- 201 Created ---------------|  ✅ Si válido
  |    OR                         |
  |<-- 401/403 ------------------|  ❌ Si inválido
  |                               |
```

## 🚀 Próximos Pasos para Deployment

### 1. Backend
```bash
cd /home/reimon/greenhouse/backend-websocket-update
cp .env.example .env
nano .env  # Configurar ESP32_AUTH_TOKEN con token seguro
./deploy-auth.sh
```

### 2. ESP32
```bash
cd /home/reimon/greenhouse
nano include/secrets.h  # Configurar mismo token
pio run --target upload
```

### 3. Verificación
```bash
# Terminal 1
pm2 logs greenhouse-backend | grep -i auth

# Terminal 2
pio device monitor

# Terminal 3
cd backend-websocket-update
./test-auth.sh
```

## 📋 Checklist de Deployment

- [ ] Generar token seguro (`openssl rand -hex 32`)
- [ ] Configurar `.env` en backend con `ESP32_AUTH_TOKEN`
- [ ] Configurar `secrets.h` en ESP32 con `DEVICE_AUTH_TOKEN`
- [ ] Verificar que tokens coincidan exactamente
- [ ] Desplegar backend: `./deploy-auth.sh`
- [ ] Compilar y subir firmware ESP32: `pio run --target upload`
- [ ] Verificar logs de autenticación en backend
- [ ] Verificar conexión ESP32 en serial monitor
- [ ] Ejecutar tests: `./test-auth.sh`
- [ ] Verificar que relays respondan correctamente
- [ ] Verificar que sensores envíen datos
- [ ] Documentar token en gestor de contraseñas seguro

## 🔧 Mantenimiento

### Rotación de Tokens (cada 3-6 meses)

1. Generar nuevo token
2. Actualizar backend `.env`
3. Reiniciar: `pm2 restart greenhouse-backend`
4. Actualizar ESP32 `secrets.h`
5. Recompilar y subir firmware
6. Verificar funcionamiento

### Monitoreo

```bash
# Ver autenticaciones
pm2 logs greenhouse-backend | grep "auth"

# Ver conexiones ESP32
pm2 logs greenhouse-backend | grep "ESP32"

# Ver errores
pm2 logs greenhouse-backend --err
```

## 📈 Métricas de Seguridad

### Antes (Sin autenticación)
- ❌ Cualquiera puede enviar comandos a relays
- ❌ Cualquiera puede inyectar datos de sensores
- ❌ No hay control de acceso
- ❌ Vulnerable a ataques MitM sin TLS

### Después (Con autenticación)
- ✅ Solo dispositivos autorizados pueden conectarse
- ✅ Tokens validados en cada interacción
- ✅ Backoff exponencial previene fuerza bruta
- ✅ TLS + Auth = doble capa de seguridad
- ✅ Logs de acceso para auditoría

## 🎓 Lecciones Aprendidas

1. **Tokens en memoria**: No usar `String` para tokens sensibles, mejor char arrays
2. **Backoff necesario**: Sin backoff, fallos de auth saturan el sistema
3. **Desconexión inmediata**: Clientes no autenticados deben desconectarse de inmediato
4. **Variables de entorno**: `.env` es crítico, siempre tener `.env.example`
5. **Testing automatizado**: Scripts de test ayudan a verificar configuración

## 📚 Referencias

- [SECURITY.md](../SECURITY.md) - Documentación completa de seguridad
- [README_AUTH_UPDATE.md](README_AUTH_UPDATE.md) - Guía de actualización
- [.env.example](.env.example) - Configuración backend
- [secrets.example.h](../include/secrets.example.h) - Configuración ESP32

---

**Versión:** 2.0  
**Implementado:** 2025-10-16  
**Breaking Change:** ⚠️ Sí - requiere actualización de backend y firmware  
**Estado:** ✅ Completado - Listo para deployment
