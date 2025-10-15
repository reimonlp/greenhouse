# API Documentation & Changelog

**Fecha de creación:** 13 de octubre de 2025  
**Versión del firmware:** Ver `FIRMWARE_VERSION` en config.h  
**Puerto API:** 80 (configurable en `API_PORT`)

---

## 📋 Índice
- [Autenticación](#autenticación)
- [Rate Limiting](#rate-limiting)
- [Endpoints de Sensores](#endpoints-de-sensores)
- [Endpoints de Relés](#endpoints-de-relés)
- [Endpoints del Sistema](#endpoints-del-sistema)
- [Endpoints de Configuración](#endpoints-de-configuración)
- [Endpoints de Logs](#endpoints-de-logs)
- [Endpoints de Firmware](#endpoints-de-firmware)
- [Endpoints de Filesystem](#endpoints-de-filesystem)
- [Endpoints de Monitoreo](#endpoints-de-monitoreo)
- [Dashboard Web](#dashboard-web)
- [Changelog](#changelog)

---

## 🔐 Autenticación

La mayoría de los endpoints requieren autenticación mediante token Bearer o parámetro de consulta.

### Métodos de autenticación:
1. **Header Authorization:** `Authorization: Bearer <token>`
2. **Query parameter:** `?token=<token>`

### Endpoints sin autenticación (públicos):
- `GET /api/sensors` - Lectura de sensores
- `GET /api/system/health` - Estado de salud del sistema
- `GET /api/healthz` - Endpoint ultra ligero de monitoreo
- `GET /api/system/uptime` - Tiempo de actividad en milisegundos
- Dashboard web (archivos estáticos)

### Rotación de token:
- **`POST /api/system/token/rotate`**
  - Parámetros: `current` (token actual), `next` (nuevo token)
  - Requiere autenticación con token actual
  - Mínimo 12 caracteres para el nuevo token

---

## ⏱️ Rate Limiting

- **Ventana:** 60 segundos
- **Máximo de peticiones:** Configurado en `MAX_API_REQUESTS`
- **Slots de IP:** `RATE_LIMIT_SLOTS` (tamaño fijo)
- **Respuesta al exceder límite:** HTTP 429 "Rate limit exceeded"
- **Diagnóstico:** `GET /api/system/ratelimit`

---

## 🌡️ Endpoints de Sensores

### 1. **Obtener datos actuales de sensores**
**`GET /api/sensors`**
- **Autenticación:** No requerida
- **Rate limit:** Sí
- **Respuesta:**
```json
{
  "timestamp": 1234567890,
  "valid": true,
  "temperature": 25.5,
  "humidity": 60.0,
  "soil_moisture_1": 45.0,
  "soil_moisture_2": 50.0,
  // temp_sensor_1/2 eliminados
  "flags": {
    "dht": true,
    "soil_complete": true,
    "ext_temps_complete": true,
    "overall_complete": true
  },
  "statistics": {
    "temp_min": 20.0,
    "temp_max": 30.0,
    "temp_avg": 25.0,
    "humidity_min": 40.0,
    "humidity_max": 80.0,
    "humidity_avg": 60.0
  }
}
```

### 2. **Obtener historial de sensores**
**`GET /api/sensors/history`**
- **Autenticación:** Requerida
- **Parámetros opcionales:**
  - `from` - Timestamp desde (unix time)
  - `to` - Timestamp hasta (unix time)
- **Respuesta:** Array de lecturas históricas

### 3. **Calibrar sensor de humedad del suelo**
**`POST /api/sensors/calibrate`**
- **Autenticación:** Requerida
- **Parámetros:**
  - `sensor` (int): 1 o 2 (índice del sensor)
  - `offset` (float): Valor de calibración offset
- **Respuesta:**
```json
{
  "success": true,
  "message": "Sensor calibrated successfully"
}
```

---

## 🔌 Endpoints de Relés

### 1. **Obtener estado de todos los relés**
**`GET /api/relays`**
- **Autenticación:** Requerida
- **Respuesta:** Estado completo del sistema de relés (estados, modos, reglas, timeouts, métricas)

### 2. **Establecer estado de un relé**
**`POST /api/relays/set`**
- **Autenticación:** Requerida
- **Parámetros:**
  - `relay` (int): Índice del relé (0-3)
  - `state` (bool/string): true/false o "1"/"0"
- **Comportamiento:** Cambia automáticamente el relé a modo MANUAL
- **Respuesta:**
```json
{
  "success": true,
  "relay": 0,
  "state": true,
  "message": "Relay state updated successfully"
}
```

### 3. **Cambiar modo de un relé**
**`POST /api/relays/mode`**
- **Autenticación:** Requerida
- **Parámetros:**
  - `relay` (int): Índice del relé (0-3)
  - `mode` (string): "auto" o "manual"
- **Respuesta:**
```json
{
  "success": true,
  "relay": 0,
  "mode": "auto",
  "message": "Relay mode updated successfully"
}
```

### 4. **Configurar regla automática**
**`POST /api/relays/rule`**
- **Autenticación:** Requerida
- **Parámetros:**
  - `relay` (int): Índice del relé (0-3)
  - `rule` (string): JSON con la configuración de la regla
- **Formato de regla:** Ver motor de reglas en `rule_engine.h`

---

## 🖥️ Endpoints del Sistema

### 1. **Estado general del sistema**
**`GET /api/system/status`**
- **Autenticación:** Requerida
- **Respuesta:** Información completa del sistema (WiFi, memoria, tiempo activo, etc.)

### 2. **Estado de salud (Health Check)**
**`GET /api/system/health`**
- **Autenticación:** No requerida
- **Respuesta ligera:**
```json
{
  "uptime": 123456,
  "free_heap": 234567,
  "min_free_heap": 200000,
  "largest_free_block": 180000,
  "min_largest_free_block": 150000,
  "fragmentation_ratio": 0.85,
  "min_fragmentation_ratio": 0.80,
  "wifi": true,
  "rssi": -45,
  "state": "NORMAL",
  "ts": 1234567890,
  "loop_avg_us": 1250,
  "wifi_reconnect_attempts": 5,
  "wifi_reconnect_successes": 5,
  "ntp_failures": 0,
  "last_wifi_reason": 200,
  "wifi_reasons": {
    "r200": 2,
    "r201": 1,
    "r202": 0,
    "r203": 0,
    "r204": 0
  }
}
```

### 3. **Estadísticas del sistema**
**`GET /api/system/stats`**
- **Autenticación:** Requerida
- **Respuesta:** Estadísticas detalladas de sensores, relés, sistema

### 4. **Features compiladas**
**`GET /api/system/features`**
- **Autenticación:** Requerida
- **Respuesta:**
```json
{
  "firmware_version": "1.0.0",
  "build_date": "2025-10-13",
  "schema_version": 1,
  "min_log_level": 1,
  "ota_disabled": false,
  "remote_db_disabled": false,
  "dashboard_fallback_disabled": false,
  "status_led": true,
  "dht_stabilize_ms": 2000
}
```

### 5. **Métricas de reglas automáticas**
**`GET /api/system/rules`**
- **Autenticación:** Requerida
- **Respuesta:** Métricas de evaluación y activación por regla

### 6. **Diagnóstico de Rate Limiter**
**`GET /api/system/ratelimit`**
- **Autenticación:** Requerida
- **Respuesta:**
```json
{
  "window_ms": 60000,
  "max_requests": 100,
  "slots_used": 5,
  "slots_capacity": 16,
  "evictions": 2,
  "entries": [
    {
      "ip": "192.168.1.100",
      "count": 45,
      "window_start_ms": 123456
    }
  ]
}
```

### 7. **Estado de GPIO**
**`GET /api/system/gpio`**
- **Autenticación:** Requerida
- **Respuesta:** Estado de pines GPIO configurados (relés, LED de estado)

### 8. **Estado de memoria heap**
**`GET /api/system/heap`**
- **Autenticación:** Requerida
- **Rate limit:** Sí
- **Respuesta ligera:** Métricas de memoria y uptime

### 9. **Nivel de log en tiempo de ejecución**
**`GET /api/system/loglevel`**
- **Autenticación:** Requerida
- **Respuesta:**
```json
{
  "current": 2,
  "compiled_min": 1
}
```

**`POST /api/system/loglevel`**
- **Autenticación:** Requerida
- **Parámetros:**
  - `level` (int): 0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR, 4=CRITICAL

### 10. **Pausar/Reanudar sistema**
**`POST /api/system/pause`**
- **Autenticación:** Requerida
- **Parámetros:**
  - `pause` (bool): true para pausar, false para reanudar
- **Efecto:** Suspende operaciones automáticas de relés

### 11. **Reiniciar sistema**
**`POST /api/system/reset`**
- **Autenticación:** Requerida
- **Efecto:** Reinicia el ESP32 después de 100ms

### 12. **Resetear configuración WiFi**
**`POST /api/system/wifi-reset`**
- **Autenticación:** Requerida
- **Efecto:** Borra credenciales WiFi y reinicia en modo AP

---

## ⚙️ Endpoints de Configuración

### 1. **Obtener configuración actual**
**`GET /api/config`**
- **Autenticación:** Requerida
- **Respuesta:**
```json
{
  "firmware_version": "1.0.0",
  "build_date": "2025-10-13",
  "api_port": 80,
  "sensor_read_interval": 5000,
  "log_interval": 60000,
  "safety_limits": {
    "max_temp": 40.0,
    "min_temp": 5.0,
    "max_humidity": 95.0,
    "min_humidity": 20.0
  }
}
```

### 2. **Establecer configuración**
**`POST /api/config`**
- **Autenticación:** Requerida
- **Estado:** No implementado (501) - Preparado para versionado de esquema

### 3. **Backup de configuración**
**`GET /api/config/backup`**
- **Autenticación:** Requerida
- **Respuesta:** Configuración completa incluyendo `schema_version`

### 4. **Restaurar configuración**
**`POST /api/config/restore`**
- **Autenticación:** Requerida
- **Body:** JSON de configuración con soporte de migración de esquema
- **Respuesta:**
```json
{
  "success": true,
  "from_version": 1,
  "to_version": 1,
  "applied": false
}
```

---

## 📝 Endpoints de Logs

### 1. **Obtener logs locales**
**`GET /api/logs`**
- **Autenticación:** Requerida
- **Parámetros opcionales:**
  - `count` (int): Número de logs (default: 50, max: 1000)
- **Respuesta:** Array de logs en formato JSON

### 2. **Obtener logs críticos**
**`GET /api/logs/critical`**
- **Autenticación:** Requerida
- **Parámetros opcionales:**
  - `count` (int): Número de logs críticos (default: 25, max: 100)

### 3. **Limpiar logs locales**
**`DELETE /api/logs/clear`**
- **Autenticación:** Requerida
- **Efecto:** Elimina todos los logs almacenados localmente

---

## 🔧 Endpoints de Firmware

### 1. **Información del firmware**
**`GET /api/firmware/info`**
- **Autenticación:** Requerida
- **Respuesta:**
```json
{
  "version": "1.0.0",
  "build_date": "2025-10-13",
  "chip_model": "ESP32",
  "chip_revision": 3,
  "flash_size": 4194304,
  "free_heap": 234567,
  "uptime": 123456
}
```

---

## 💾 Endpoints de Filesystem

### 1. **Formatear filesystem**
**`POST /api/fs/format`**
- **Autenticación:** Requerida
- **Parámetros:**
  - `confirm` (string): Debe ser exactamente "YES"
- **Efecto:** Formatea LittleFS y reinicia el dispositivo
- **⚠️ ADVERTENCIA:** Destruye todos los datos almacenados

---

## 📊 Endpoints de Monitoreo

### 1. **Health check ultra ligero**
**`GET /api/healthz`**
**`HEAD /api/healthz`**
- **Autenticación:** No requerida
- **Respuesta:** HTTP 200 "ok" (texto plano)
- **Uso:** Monitoreo externo, checks de disponibilidad

### 2. **Uptime simple**
**`GET /api/system/uptime`**
- **Autenticación:** No requerida
- **Respuesta:** Número de milisegundos desde el arranque (texto plano)

### 3. **Métricas estilo Prometheus**
**`GET /metrics`**
- **Autenticación:** Requerida
- **Formato:** Prometheus exposition format
- **Métricas incluidas:**
  - `system_uptime_milliseconds` (counter)
  - `system_free_heap_bytes` (gauge)
  - `system_min_free_heap_bytes` (gauge)
  - `system_loop_avg_us` (gauge)
  - `wifi_reconnect_attempts` (counter)
  - `wifi_reconnect_successes` (counter)
  - `ntp_failures` (counter)
  - `wifi_last_disconnect_reason` (gauge)
  - `wifi_disconnect_reasons_total{reason="X"}` (counter)
  - `rule_evaluations_total{relay="X"}` (counter)

---

## 🌐 Dashboard Web

El sistema incluye un dashboard web completo servido desde archivos estáticos:

### Archivos disponibles:
- `/` o `/index.html` - Dashboard principal
- `/settings.html` - Página de configuración
- `/logs.html` - Visualización de logs
- `/script.js` - Lógica del frontend
- `/style.css` - Estilos CSS
- `/favicon.svg` - Icono del sitio
- `/404.html` - Página de error 404

### Features del Dashboard:
- Visualización en tiempo real de sensores
- Control de relés (manual/auto)
- Configuración de reglas automáticas
- Visualización de estadísticas
- Gestión de logs
- Información del sistema

---

## 🔒 Seguridad

### Recomendaciones:
1. **Token fuerte:** Mínimo 24 caracteres aleatorios
2. **HTTPS:** Considerar reverse proxy con TLS
3. **Red segura:** No exponer directamente a Internet
4. **Rate limiting:** Protección contra abuso integrada
5. **Token hashing:** SHA-256 interno (mbedtls si disponible)

### Advertencias de seguridad:
El sistema registra automáticamente advertencias si:
- Token parece placeholder ("tu_token_secreto_aqui", "REPLACE_ME_TOKEN")
- Token tiene menos de 12 caracteres

---

## 📅 Changelog

### [v2.0.0] - 2025-10-13
#### 🎨 Refactorización Completa de la Interfaz Web

**Cambios Mayores:**
- ✨ **Nueva arquitectura de interfaz**: Diseño modular con sidebar de navegación
- 🎨 **Sistema de diseño moderno**: Tema oscuro con gradientes verdes, glassmorphism, animaciones suaves
- 📱 **Totalmente responsive**: Sidebar colapsable, diseño adaptativo para móviles y tablets
- ⚡ **Mejor rendimiento**: Vistas lazy-loading, optimización de renders
- 🔐 **UX mejorada de autenticación**: Modal centralizado, indicadores visuales de estado

**Estructura de Navegación:**
1. **Dashboard** - Vista general con métricas rápidas y gráficas
2. **Sensores** - Monitoreo detallado de sensores ambientales
3. **Control** - Gestión de relés con autenticación requerida
4. **Sistema** - Información de hardware, red y firmware
5. **Logs** - Visualización de registros con filtros avanzados

**Componentes Visuales:**
- Status cards con iconos y gradientes personalizados por tipo
- Sensor cards con indicadores de estado en tiempo real
- Charts mejorados con controles interactivos
- Info lists con diseño limpio y legible
- Modals con animaciones y backdrop blur

**CSS Highlights:**
- Variables CSS para theming consistente
- Transiciones suaves (150ms-500ms)
- Sombras y efectos de profundidad
- Scrollbar personalizado
- Loading states y animaciones
- Media queries para 3 breakpoints (1024px, 768px, móvil)

**Nuevo Favicon:**
- SVG optimizado con diseño de invernadero
- Indicadores visuales de temperatura y humedad
- Gradientes modernos matching el tema de la aplicación

#### 📖 Nueva Documentación

**HTTPS_SETUP.md - Guía completa de configuración HTTPS**
- ✅ **Opción 1:** Certificado autofirmado en ESP32 (red local)
- ✅ **Opción 2:** Reverse proxy con Nginx + Let's Encrypt (producción)
- ✅ **Opción 3:** mDNS + certificado raíz local (simplificado)
- 🔧 Scripts de conversión de certificados
- 🔒 Headers de seguridad (HSTS, CSP, etc.)
- 📝 Configuración específica para IP 10.0.0.104

**Características destacadas de las opciones HTTPS:**
- Certificado autofirmado: Configuración en < 10 minutos
- Reverse proxy: Certificado válido sin advertencias del navegador
- mDNS: Acceso vía `https://greenhouse.local/`
- Port forwarding y configuración de router documentada
- Script Python para convertir .pem a arrays C++

#### 🔧 Configuración y Compatibilidad

**Archivos Modificados:**
- ✏️ `data/index.html` - Reescrito completamente (estructurado por vistas)
- ✏️ `data/style.css` - Reescrito desde cero con sistema de diseño moderno
- ✏️ `data/favicon.svg` - Nuevo diseño temático de greenhouse
- 📄 `data/*.backup` - Backups de archivos originales creados

**Archivos Nuevos:**
- 📄 `HTTPS_SETUP.md` - Guía completa de HTTPS
- 📄 `API_CHANGELOG.md` - Este archivo (actualizado)

**JavaScript (Pendiente):**
- ⏳ Refactorización modular en progreso
- ⏳ Separación por vistas
- ⏳ Optimización de polling
- ⏳ Cache inteligente

#### ⚠️ Breaking Changes

**Frontend:**
- IDs de elementos HTML cambiados (nueva convención de nombres)
- Clases CSS completamente nuevas
- Estructura DOM reorganizada

**Compatibilidad hacia atrás:**
- ✅ API endpoints sin cambios
- ✅ Autenticación compatible
- ✅ WebSocket compatible
- ✅ Formato de datos sin cambios

#### 🎯 Próximos Pasos

**Tareas Pendientes:**
- [ ] Finalizar refactorización de `script.js`
- [ ] Implementar PWA manifest.json
- [ ] Añadir modo offline
- [ ] Implementar opción HTTPS elegida
- [ ] Optimizar assets (minificación, compresión gzip)
- [ ] Testing en múltiples navegadores
- [ ] Documentar guía de usuario final

---

### [v1.0.0] - 2025-10-13
#### Documentación inicial
- ✅ Documentación completa de todos los endpoints API
- ✅ 45+ endpoints identificados y documentados
- ✅ Categorización por funcionalidad
- ✅ Ejemplos de respuesta para endpoints principales
- ✅ Información de autenticación y seguridad
- ✅ Documentación de rate limiting
- ✅ Métricas estilo Prometheus

#### Features destacadas identificadas:
- Sistema de autenticación con token rotable
- Rate limiting por IP con diagnóstico
- Migración de esquema de configuración
- Múltiples endpoints de health check (ligero/completo)
- Soporte para formato Prometheus
- Control runtime del nivel de log
- Diagnóstico de GPIO y memoria
- Backup/restore de configuración
- Dashboard web integrado

---

## 📚 Referencias

### Archivos relacionados:
- `include/api.h` - Definiciones de la clase APIManager
- `src/api.cpp` - Implementación de todos los endpoints
- `include/config.h` - Constantes de configuración
- `include/secrets.h` - Token API y credenciales
- `data/*` - Archivos del dashboard web

### Constantes importantes:
- `API_PORT` - Puerto del servidor API
- `API_TOKEN` - Token de autenticación
- `MAX_API_REQUESTS` - Límite de peticiones por ventana
- `RATE_LIMIT_SLOTS` - Slots para tracking de IPs
- `CONFIG_SCHEMA_VERSION` - Versión del esquema de configuración

---

**Nota:** Este documento se actualizará con cada cambio en la API. Mantener sincronizado con el código fuente.
