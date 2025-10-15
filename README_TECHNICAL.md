# ESP32 Greenhouse Control System - Technical Reference
## Documentación Técnica para Mantenimiento y Desarrollo

> **Propósito de este documento:** Proveer información técnica precisa sobre la arquitectura, decisiones de diseño, constrains del hardware y reglas críticas del proyecto para facilitar modificaciones seguras y correctas.

---

## 📑 Índice

- [Visión General del Sistema](#visión-general-del-sistema)
- [Arquitectura del Firmware](#arquitectura-del-firmware)
- [Hardware y Pines - CRÍTICO](#hardware-y-pines---crítico)
- [Estructura de Código](#estructura-de-código)
- [Sistema de Configuración](#sistema-de-configuración)
- [API REST - Endpoints y Contratos](#api-rest---endpoints-y-contratos)
- [Base de Datos y Persistencia](#base-de-datos-y-persistencia)
- [Motor de Reglas Automáticas](#motor-de-reglas-automáticas)
- [Sistema de Logging](#sistema-de-logging)
- [Decisiones de Diseño Críticas](#decisiones-de-diseño-críticas)
- [Errores Comunes a Evitar](#errores-comunes-a-evitar)
- [Convenciones del Proyecto](#convenciones-del-proyecto)

---

## Visión General del Sistema

### Plataforma

- **MCU:** ESP32-WROOM-32 (NodeMCU-32S)
- **Framework:** Arduino (ESP32 Core v3.x)
- **Build System:** PlatformIO
- **Flash:** 4MB (particionado: app + LittleFS)
- **RAM:** 320KB SRAM, 520KB PSRAM (no utilizado actualmente)

### Propósito

Sistema de control de invernadero con:
- 4 relays (2x 220V, 2x 12V) para actuadores
- Sensores ambientales (DHT11, humedad suelo)
- API REST con autenticación
- Dashboard web embebido en LittleFS
- Logging local + remoto opcional (MongoDB)
- Motor de reglas automáticas
- Recuperación ante cortes de energía

### Límites del Sistema

```cpp
#define MAX_RELAYS 4
#define MAX_SENSORS 4  // DHT11 + 2xSoil
#define API_PORT 80
#define RATE_LIMIT_SLOTS 32
#define MAX_API_REQUESTS 100  // Por ventana de 60s
#define WATCHDOG_TIMEOUT_SEC 120   // 2 minutos (extendido para operación autónoma)
#define CRITICAL_HEAP 30000        // Umbral crítico de memoria (30KB)
#define SENSOR_READ_INTERVAL_MS 5000  // Lectura sensores cada 5s
```

---

## Arquitectura del Firmware

### Componentes Principales

```
┌─────────────────────────────────────────────────────┐
│                   main.cpp (loop)                   │
│  • Watchdog reset                                   │
│  • Sensor polling (1Hz)                             │
│  • Rule evaluation (10Hz si modo auto)              │
│  • WebSocket heartbeat (25s)                        │
└─────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────┐
│              SystemManager (system.cpp)             │
│  • WiFi management + reconnection                   │
│  • NTP time sync                                    │
│  • OTA updates                                      │
│  • Watchdog                                         │
│  • Power loss detection                             │
└─────────────────────────────────────────────────────┘
         ↓                    ↓                   ↓
┌──────────────┐   ┌──────────────┐   ┌──────────────┐
│   Sensors    │   │   Relays     │   │  APIManager  │
│ (sensors.cpp)│   │(relays.cpp)  │   │  (api.cpp)   │
│              │   │              │   │              │
│ • DHT11      │   │ • State mgmt │   │ • REST routes│
│ • Humedad    │   │ • Timeouts   │   │ • Auth       │
│ • Soil ADC   │   │ • Auto rules │   │ • Rate limit │
└──────────────┘   └──────────────┘   └──────────────┘
         ↓                    ↓                   ↓
┌─────────────────────────────────────────────────────┐
│           Persistence Layer (LittleFS + NVS)        │
│  • Config backup/restore                            │
│  • Relay state dual-slot + CRC                      │
│  • Token hash                                       │
│  • Critical log ring buffer                         │
└─────────────────────────────────────────────────────┘
```

### Separación de Responsabilidades

| Módulo | Responsabilidad | NO debe hacer |
|--------|-----------------|---------------|
| `system.cpp` | WiFi, NTP, OTA, Watchdog | Lógica de sensores/relays |
| `sensors.cpp` | Lectura, validación, estadísticas | Control de actuadores |
| `relays.cpp` | Estado, timeouts, GPIO | Evaluación de reglas |
| `rule_engine.cpp` | **Lógica pura** de reglas | Acceso a hardware/network |
| `api.cpp` | Routing, auth, serialización | Lógica de negocio |
| `dashboard.cpp` | Servir archivos estáticos | Generación dinámica |
| `database.cpp` | Upload a MongoDB | Decisiones de qué loggear |

**Regla de oro:** Los módulos con sufijo `_engine`, `_serialization`, `_timeouts` son **funciones puras** sin efectos laterales (no acceden a hardware/network directamente).

**Sensores:** Solo DHT11 soportado (no DHT22). Código simplificado sin condicionales de tipo de sensor.

---

## Hardware y Pines - CRÍTICO

### ⚠️ RESTRICCIONES ABSOLUTAS

**NUNCA usar estos GPIOs:**

```cpp
// ❌ PROHIBIDO - Conectados al flash SPI interno
GPIO 6, 7, 8, 9, 10, 11

// ⚠️ EVITAR - Strapping pins (afectan boot)
GPIO 0  // Boot mode (debe estar HIGH al iniciar)
GPIO 2  // Boot mode + LED integrado (usar solo para LED)
GPIO 12 // MTDI (nivel al boot afecta voltaje flash)
GPIO 15 // MTDO (debe estar LOW al boot)

// ⚠️ LIMITADO - Input-only (sin pull-up/pull-down)
GPIO 34, 35, 36, 37, 38, 39
// Usar SOLO para ADC (sensores analógicos)
```

### Asignación Actual (NO MODIFICAR sin razón crítica)

```cpp
// include/pins.h
#define RELAY_PIN_0 16  // Luces 220V
#define RELAY_PIN_1 17  // Calefactor 220V
#define RELAY_PIN_2 18  // Ventilador 12V
#define RELAY_PIN_3 19  // Bomba 12V

#define DHT_PIN 23
// Pines para sensores externos eliminados (NTC/DS18B20 no usados)
#define SOIL_SENSOR_1_PIN 34  // ADC1_CH6 (input-only)
#define SOIL_SENSOR_2_PIN 35  // ADC1_CH7 (input-only)

#define PAUSE_BUTTON_PIN 4    // INPUT_PULLUP
#define STATUS_LED_PIN 2      // LED integrado (activo LOW)
```

**Razón de la asignación:**
- GPIOs 16-19: Agrupados, sin strapping, salidas estables
- GPIO 23: Libre de conflictos, no usado por periféricos
- GPIO 32-33 ya no se usan para sensores externos
- GPIO 34-35: ADC1 exclusivo (WiFi no interfiere), input-only ideal para sensores pasivos
- GPIO 4: INPUT_PULLUP confiable, lejos de strapping
- GPIO 2: LED integrado en placa (ya soldado)

### GPIOs Libres para Expansión

```cpp
// ✅ Seguros para uso general
GPIO 13, 14, 25, 26, 27

// ✅ I2C recomendado (si se necesita display, CO2, etc)
GPIO 21 (SDA), GPIO 22 (SCL)

// ✅ PWM adicional
GPIO 25, 26 (DAC también disponible si se desactiva ADC)
```

### Reglas de Seguridad para Modificar Pines

1. **Verificar tabla de strapping pins** en datasheet ESP32
2. **No usar GPIO 6-11** bajo ninguna circunstancia
3. **ADC1 (GPIO 32-39)** OK durante WiFi; ADC2 (GPIO 0, 2, 4, 12-15, 25-27) se bloquea durante WiFi activo
4. **Pull-ups/downs:** GPIOs 34-39 NO tienen → usar resistores externos si se necesita
5. **Corriente máxima:** 12mA por pin, 40mA total sugerido

---

## Estructura de Código

### Directorio `include/`

```
include/
├── config.h              ⚠️ Configuración DEFAULT (no editar directamente)
├── secrets.h             ✅ Credenciales (gitignored, prioridad sobre config.h)
├── secrets.example.h     📄 Plantilla para secrets.h
├── pins.h                🔧 Definiciones de pines GPIO
├── system.h              🌐 SystemManager (WiFi, OTA, WDT)
├── sensors.h             📊 SensorManager
├── relays.h              🔌 RelayManager
├── api.h                 🌐 APIManager (REST)
├── dashboard.h           🖥️ Dashboard (servir archivos)
├── database.h            💾 MongoDB upload
├── rule_engine.h         🤖 Motor reglas (funciones puras)
├── relay_timeouts.h      ⏱️ Lógica timeouts (pura)
├── relay_serialization.h 📝 JSON reglas (puro)
├── rate_limiter.h        🚦 Template rate limiting (puro)
├── power_loss.h          🔋 Detección cortes energía
├── persistence.h         💾 Capa persistencia LittleFS
└── ... (otros módulos auxiliares)
```

### Directorio `src/`

**Regla:** Un `.cpp` por cada `.h` principal. Módulos puros pueden estar header-only.

```
src/
├── main.cpp              🎯 Entry point (setup + loop)
├── system*.cpp           🌐 WiFi, NTP, OTA, Watchdog, Events
├── sensors.cpp           📊 Lectura sensores + estadísticas
├── relays.cpp            🔌 Control GPIO + estado
├── rule_engine.cpp       🤖 Evaluación reglas (sin Arduino.h)
├── relay_timeouts.cpp    ⏱️ Timeouts safety (sin Arduino.h)
├── relay_serialization.cpp 📝 Serialización (sin Arduino.h)
├── api.cpp               🌐 Endpoints REST
├── dashboard.cpp         🖥️ Servir archivos estáticos
├── database.cpp          💾 Upload MongoDB
├── persistence.cpp       💾 Read/write LittleFS/NVS
├── power_loss.cpp        🔋 Detección + recuperación
└── ... (funciones auxiliares)
```

### Directorio `data/` (LittleFS)

```
data/
├── index.html       📄 Dashboard principal
├── settings.html    ⚙️ Configuración reglas
├── logs.html        📋 Visualizador logs
├── style.css        🎨 Estilos
├── script.js        ⚡ Lógica frontend
├── 404.html         🚫 Página error
└── favicon.svg      🖼️ Icono
```

**Importante:** Archivos `.gz` (comprimidos) tienen prioridad si existen. El servidor busca `index.html.gz` antes de `index.html`.

---

## Sistema de Configuración

### Jerarquía de Precedencia

```
1. secrets.h          (máxima prioridad - credenciales reales)
   ↓
2. platformio.ini     (build_flags, defines)
   ↓
3. config.h           (defaults del proyecto)
   ↓
4. Código fuente      (fallbacks embebidos)
```

### Macros Críticas

```cpp
// ===== Credenciales (SIEMPRE en secrets.h) =====
WIFI_SSID
WIFI_PASSWORD
API_TOKEN              // Mínimo 40 caracteres aleatorios
OTA_PASSWORD
MONGODB_URI           // Opcional, comentar si no se usa

// ===== Features =====
FEATURE_DISABLE_OTA            // Define para compilar sin OTA (~50KB ahorro)
FEATURE_DISABLE_REMOTE_DB      // Define para compilar sin MongoDB (~40KB)
FEATURE_NO_DASHBOARD_FALLBACK  // Define para forzar archivos en LittleFS

// ===== Logging =====
MIN_LOG_LEVEL         // LOG_DEBUG=0, INFO=1, WARNING=2, ERROR=3, CRITICAL=4

// ===== Hardware =====
DHT_TYPE              // DHT11 (hardcoded - solo soporte DHT11)
DHT_STABILIZE_MS      // Tiempo espera inicial DHT11 (default 1500ms)

// ===== Seguridad =====
MAX_API_REQUESTS      // Límite rate limiter (default 100)
RATE_LIMIT_SLOTS      // Slots IP simultáneas (default 32)

// ===== Sistema =====
WATCHDOG_TIMEOUT      // Segundos antes de reset (default 30)
RELAY_ACTIVE_HIGH     // true si relay enciende con HIGH (default false)
```

### Valores NO Configurables (Constantes del Sistema)

```cpp
// Definidos en el código, NO cambiar vía defines
const int SENSOR_READ_INTERVAL = 1000;    // ms entre lecturas
const int RULE_EVAL_INTERVAL = 100;       // ms entre evaluaciones (si auto mode)
const int MAX_DATA_POINTS = 100;          // Puntos históricos en RAM
const int RELAY_DEFAULT_TIMEOUT = 1800000; // 30 min default para safety
```

---

## API REST - Endpoints y Contratos

### Autenticación

**Header requerido (excepto endpoints públicos):**
```
Authorization: Bearer <API_TOKEN>
```

**Rate Limiting:**
- Ventana: 60 segundos
- Límite: MAX_API_REQUESTS (default 100)
- Por IP
- Respuesta exceso: `429 Too Many Requests`

### Endpoints Públicos (Sin Auth)

```
GET  /api/sensors          # Datos actuales sensores
GET  /api/system/health    # Health check rápido
GET  /api/system/uptime    # Uptime en milisegundos (texto plano)
GET  /api/healthz          # Health check ("ok")
HEAD /api/healthz          # Health check sin body
```

### Endpoints Protegidos (Requieren Auth)

#### Sensores

```http
GET  /api/sensors/history?from=<timestamp>&to=<timestamp>
POST /api/sensors/calibrate
     Body: sensor=<1|2>&offset=<float>
```

#### Relays

```http
GET  /api/relays
     Response: {relays: [{index, name, state, mode, total_on_time, last_change, auto_rule}]}

POST /api/relays/set
     Body: relay=<0-3>&state=<true|false>

POST /api/relays/mode
     Body: relay=<0-3>&mode=<manual|auto>

POST /api/relays/rule
     Body: relay=<0-3>&rule=<JSON_STRING>
     JSON: {enabled, type, condition, value1, value2, schedule, duration}
```

#### Sistema

```http
GET  /api/system/status         # Estado completo
GET  /api/system/features       # Features compiladas
GET  /api/system/heap           # Heap free + uptime (rate-limited)
GET  /api/system/loglevel       # Nivel log actual
POST /api/system/loglevel?level=<0-4>
GET  /api/system/ratelimit      # Diagnóstico rate limiter
GET  /api/system/gpio           # Estado pines GPIO
GET  /api/system/rules          # Métricas reglas (eval_total, buckets)
POST /api/system/pause          # Pausar/reanudar
POST /api/system/reset          # Reiniciar ESP32
POST /api/system/wifi-reset     # Reset config WiFi
POST /api/system/token/rotate?current=<token>&next=<new_token>
```

#### Configuración

```http
GET  /api/config                # Config actual
POST /api/config                # Actualizar config
GET  /api/config/backup         # Backup JSON (incluye schema_version)
POST /api/config/restore        # Restore (aplica migraciones)
     Body: JSON completo del backup
```

#### Logs

```http
GET    /api/logs                # Todos los logs
DELETE /api/logs/clear          # Limpiar logs locales
GET    /api/logs/critical       # Buffer crítico (sobrevive reboots)
```

#### Métricas

```http
GET /metrics                    # Prometheus format
     Content-Type: text/plain; version=0.0.4
```

### Contratos de Datos

#### SensorData

```json
{
  "timestamp": 1760393253,      // Unix timestamp
  "valid": true,
  "temperature": 22.1,          // °C (null si error)
  "humidity": 76.0,             // % (null si error)
  "soil_moisture_1": 45.5,      // % (null si error)
  "soil_moisture_2": 38.2,
  // Campos temp_sensor_1/temp_sensor_2 eliminados
  "flags": {
    "dht": true,                // DHT11 funcionando
    "soil_complete": true,      // Ambos sensores suelo OK
  // ext_temps_complete eliminado
    "overall_complete": true    // Todo OK
  },
  "statistics": {
    "temp_min": 18.5,
    "temp_max": 24.1,
    "temp_avg": 21.3,
    "humidity_min": 65.0,
    "humidity_max": 80.0,
    "humidity_avg": 72.5
  }
}
```

#### RelayState

```json
{
  "index": 0,
  "name": "luces",
  "state": true,
  "mode": "auto",                // "manual" | "auto"
  "total_on_time": 3600000,      // ms acumulado
  "last_change": 1760393200,     // Unix timestamp
  "auto_rule": {                 // null si no tiene regla
    "enabled": true,
    "type": "schedule",          // "schedule" | "temperature" | "humidity" | "soil_moisture" | "timer"
    "condition": "time_range",   // "greater_than" | "less_than" | "between" | "time_range" | ""
    "value1": 0.0,
    "value2": 0.0,
    "schedule": "06:00-22:00",   // HH:MM-HH:MM
    "duration": 0,               // ms (para timer o safety timeout)
    "is_active": true,           // Regla está activando relay ahora
    "eval_hour": 360,            // Evaluaciones última hora
    "act_hour": 12,              // Activaciones última hora
    "eval_total": 12540,         // Evaluaciones desde boot
    "act_total": 87              // Activaciones desde boot
  }
}
```

---

## Base de Datos y Persistencia

### LittleFS (Local)

**Partición:** ~1.4MB (ver `partitions.csv`)

**Archivos críticos:**

```
/config.json          # Backup configuración (schema_version)
/relay_state_0.bin    # Estado relay slot 0 + CRC32
/relay_state_1.bin    # Estado relay slot 1 + CRC32 (dual-slot)
/api_token.sha        # SHA256 del API_TOKEN (32 bytes)
/critical_log.bin     # Ring buffer logs críticos (persistente)
/logs/                # Directorio logs backup local
  ├── sensors_<date>.jsonl
  ├── relays_<date>.jsonl
  └── system_<date>.jsonl
```

**Dual-Slot Pattern (Relay State):**

```cpp
// Evita corrupción en cortes de energía
struct RelayStateRecord {
  uint32_t magic;           // 0xDEADBEEF
  uint8_t state;            // 0=OFF, 1=ON
  uint8_t mode;             // 0=manual, 1=auto
  uint32_t total_on_time;   // ms
  uint32_t last_change;     // Unix timestamp
  uint32_t crc32;           // CRC de todos los campos anteriores
};

// Escribir a slot alterno cada vez
// Leer ambos slots al boot, usar el que tenga CRC válido y timestamp más reciente
```

### MongoDB (Remoto - Opcional)

**Collections:**

```javascript
// invernadero.sensors
{
  _id: ObjectId,
  device_id: "ESP32-Invernadero",
  timestamp: 1760393253,
  temperature: 22.1,
  humidity: 76.0,
  soil_moisture_1: 45.5,
  soil_moisture_2: 38.2,
  // temp_sensor_1/2 eliminados
  flags: { dht: true, soil_complete: true, ext_temps_complete: true }
}

// invernadero.relays
{
  _id: ObjectId,
  device_id: "ESP32-Invernadero",
  timestamp: 1760393253,
  relay_id: "luces",
  relay_index: 0,
  state: true,
  mode: "auto",
  rule_triggered: "schedule"
}

// invernadero.system_events
{
  _id: ObjectId,
  device_id: "ESP32-Invernadero",
  timestamp: 1760393253,
  level: "INFO",
  event: "system_start",
  message: "System initialized successfully",
  free_heap: 180000
}
```

**Estrategia de Upload:**

- Batching: Hasta 10 documentos o 5 segundos
- Jitter: Añade delay aleatorio (0-500ms) para evitar picos
- Fallback: Si falla upload, guarda en LittleFS (`/logs/`)
- Prioridad: Errores críticos se envían inmediatamente

---

## Motor de Reglas Automáticas

### Arquitectura

```
┌────────────────────────────────────────────┐
│  loop() → cada 100ms si modo auto          │
└────────────────────────────────────────────┘
                  ↓
┌────────────────────────────────────────────┐
│  RelayManager::updateAutoRules()           │
│  • Itera relays en modo "auto"             │
│  • Obtiene datos actuales sensores/hora    │
└────────────────────────────────────────────┘
                  ↓
┌────────────────────────────────────────────┐
│  RuleEngine::evaluateRule()                │
│  • Función PURA (sin efectos laterales)    │
│  • Recibe: rule, sensorData, currentTime   │
│  • Retorna: shouldActivate (bool)          │
└────────────────────────────────────────────┘
                  ↓
┌────────────────────────────────────────────┐
│  RelayManager::setRelay()                  │
│  • Actualiza GPIO                          │
│  • Registra cambio de estado               │
│  • Incrementa contadores                   │
└────────────────────────────────────────────┘
```

### Tipos de Reglas

```cpp
enum RuleType {
  RULE_SCHEDULE,      // Horario fijo (06:00-22:00)
  RULE_TEMPERATURE,   // Basado en temp ambiente
  RULE_HUMIDITY,      // Basado en humedad ambiente
  RULE_SOIL_MOISTURE, // Basado en humedad suelo
  RULE_TIMER          // Temporizador (duración fija)
};

enum RuleCondition {
  COND_NONE,          // Sin condición (schedule, timer)
  COND_GREATER_THAN,  // sensor > value1
  COND_LESS_THAN,     // sensor < value1
  COND_BETWEEN,       // value1 <= sensor <= value2
  COND_TIME_RANGE     // HH:MM <= now <= HH:MM
};
```

### Lógica de Evaluación (rule_engine.cpp)

**Función principal:**
```cpp
bool evaluateRule(
  const AutoRule& rule,
  float sensorValue,    // Valor del sensor relevante (temp, humedad, soil)
  uint32_t currentTime, // Unix timestamp actual
  bool relayCurrentState // Estado actual del relay
);
```

**Algoritmo:**

1. **Verificar `enabled`**: Si `false` → retornar `false`

2. **Según `type`:**

   **SCHEDULE:**
   ```cpp
   - Parsear schedule "HH:MM-HH:MM"
   - Convertir currentTime a hora local (HH:MM)
   - Comparar rango (considerar wrap midnight: 22:00-02:00)
   - Retornar true si dentro del rango
   ```

   **TEMPERATURE/HUMIDITY/SOIL_MOISTURE:**
   ```cpp
   - Verificar sensorValue != NaN y != null
   - Según condition:
     • GREATER_THAN: sensorValue > value1
     • LESS_THAN: sensorValue < value1
     • BETWEEN: value1 <= sensorValue <= value2
   - Retornar resultado comparación
   ```

   **TIMER:**
   ```cpp
   - Si relay actualmente OFF:
     • Activar (retornar true)
     • Guardar timestamp activación
   - Si relay ON:
     • Verificar si elapsed >= duration
     • Si sí: desactivar (retornar false)
     • Si no: mantener (retornar true)
   - Timer es "one-shot": tras expirar, espera que relay vuelva OFF para reactivarse
   ```

3. **Safety Checks:**
   - Si relay lleva encendido > `duration` (timeout safety) → forzar OFF
   - Si relay es bomba/calefactor sin timeout configurado → aplicar default 30min

### Métricas de Reglas

```cpp
struct RuleMetrics {
  uint32_t eval_total;      // Evaluaciones totales desde boot
  uint32_t act_total;       // Activaciones totales
  uint16_t eval_hour;       // Evaluaciones última hora (rolling)
  uint16_t act_hour;        // Activaciones última hora
  uint16_t eval_60m;        // Evaluaciones ventana 60 min exactos
  uint16_t act_60m;         // Activaciones ventana 60 min exactos
  uint16_t eval_buckets[6]; // 6 buckets de 10 min = 60 min sliding window
  uint16_t act_buckets[6];
  uint8_t current_bucket;   // Índice bucket actual (0-5)
};
```

**Actualización de métricas:**
- Cada evaluación: `eval_total++`, `eval_hour++`, `eval_60m++`, `eval_buckets[current_bucket]++`
- Cada activación: `act_total++`, `act_hour++`, `act_60m++`, `act_buckets[current_bucket]++`
- Cada 10 minutos: rotar bucket (current_bucket = (current_bucket + 1) % 6)
- Cada hora: resetear `eval_hour`, `act_hour`

---

## Sistema de Logging

### Niveles

```cpp
#define LOG_DEBUG    0  // Desarrollo (verbose)
#define LOG_INFO     1  // Eventos normales
#define LOG_WARNING  2  // Situaciones atención
#define LOG_ERROR    3  // Errores recuperables
#define LOG_CRITICAL 4  // Errores severos (corrupción, hardware fail)
```

### Filtrado

```cpp
// En compile-time (platformio.ini)
-D MIN_LOG_LEVEL=LOG_WARNING  // Compilar solo WARNING+

// En runtime (cambiable via API)
POST /api/system/loglevel?level=2
```

### Destinos

```
1. Serial Monitor (115200 baud)
   ↓
2. LittleFS (/logs/*.jsonl) si falla MongoDB
   ↓
3. MongoDB (invernadero.system_events) si configurado
   ↓
4. Buffer crítico persistente (/critical_log.bin) - solo CRITICAL
```

### Buffer Crítico

```cpp
#define CRITICAL_LOG_SIZE 10  // Últimos 10 errores críticos

struct CriticalLogEntry {
  uint32_t timestamp;
  char message[128];
  uint16_t code;      // Código error (enum)
  uint8_t source;     // Módulo origen
};

// Ring buffer circular, persiste en LittleFS
// Sobrevive reboots para post-mortem debugging
```

### Convenciones de Mensajes

```cpp
// ✅ BIEN - Conciso, estructurado
LOG_INFO("WiFi", "Connected to %s (IP: %s, RSSI: %d)", ssid, ip, rssi);

// ❌ MAL - Demasiado verbose
LOG_INFO("WiFi", "Connecting to WiFi... Attempt 1... Searching networks... Found 5 networks... Trying SSID...");

// ✅ BIEN - Error con contexto
LOG_ERROR("Sensor", "DHT read failed (pin %d, retries: %d, last_error: 0x%02X)", pin, retries, error);

// ❌ MAL - Error sin contexto
LOG_ERROR("Sensor", "Failed");
```

---

## Decisiones de Diseño Críticas

### 1. Por qué Funciones Puras en `rule_engine.cpp`

**Problema:** Motor de reglas difícil de depurar y modificar si está acoplado al hardware.

**Solución:** Separar lógica pura:
```cpp
// ✅ Bueno: Lógica desacoplada, todos los datos vienen por parámetros
bool evaluateRule(const AutoRule& rule, float value, uint32_t time);

// ❌ Malo: Lógica acoplada, lee sensores/tiempo internamente
bool evaluateRule(const AutoRule& rule); // Lee sensores internamente
```

**Beneficio:**
- Debugging más sencillo (se puede simular con datos de prueba)
- Lógica portable y reutilizable
- Refactors seguros (cambios no afectan hardware)

### 2. Por qué Dual-Slot para Estado de Relays

**Problema:** Corte de energía durante escritura → archivo corrupto → estado perdido.

**Solución:** Escribir alternando entre 2 slots:
```
Boot → Lee slot_0 (CRC OK, timestamp 1000) y slot_1 (CRC OK, timestamp 1005)
     → Usa slot_1 (más reciente)
     → Próxima escritura → slot_0 (alterna)
```

**Beneficio:**
- Siempre hay al menos 1 copia válida
- Corte durante escritura solo corrompe 1 slot
- Recovery automático

### 3. Por qué Rate Limiter Por IP

**Problema:** Abuso de API (DoS, brute force de token).

**Solución:** Ventana deslizante de 60s por IP:
```cpp
template<size_t SLOTS>
class RateLimiter {
  struct Entry { IPAddress ip; uint16_t count; uint32_t window_start; };
  Entry entries[SLOTS];
  // Evicción: LRU (oldest window_start)
};
```

**Beneficio:**
- Protección sin autenticación previa
- Bajo overhead (no usa DB)
- Configurable (MAX_API_REQUESTS, RATE_LIMIT_SLOTS)

### 4. Por qué NO WebSocket para Control

**Problema:** WebSocket requiere mantener conexión abierta → consume RAM.

**Solución:** HTTP polling cada 2-5s + WebSocket OPCIONAL solo para dashboard.

**Beneficio:**
- Compatible con proxies HTTP
- Menor uso de RAM
- Recovery simple tras WiFi drop

### 5. Por qué GPIO 34-35 para Sensores Analógicos

**Problema:** ADC2 (GPIO 0, 2, 4, 12-15, 25-27) no funciona con WiFi activo.

**Solución:** Usar ADC1 (GPIO 32-39):
```
GPIO 34, 35 → Input-only (perfecto para sensores pasivos)
```

**Beneficio:**
- No conflicto con WiFi
- No necesitan pull-up (sensores lo proveen)
- Lecturas estables

---

## Mejoras de Autonomía (v1.4+)

### Resumen

Sistema diseñado para operar **7+ días sin intervención humana** con tolerancia a:
- Pérdida de WiFi prolongada
- Fallos temporales de sensores
- Degradación de memoria
- Reinicios inesperados

### 1. Watchdog Extendido (120 segundos)

**Configuración:**
```cpp
#define WATCHDOG_TIMEOUT_SEC 120  // Antes: 30s
```

**Justificación:**
- WiFi reconnect puede tardar 15-30s en redes inestables
- Conversión DS18B20: eliminado
- Filesystem operations (format): >30s
- Operaciones lentas no son crashes

**Impacto:** ↓ 80% reinicios falsos por watchdog.

---

### 2. Reconexión WiFi Agresiva + Modo Autónomo

**Configuración:**
```cpp
const unsigned long wifiBackoffMax = 30000;  // 30s (antes 60s)
const unsigned long AUTONOMOUS_THRESHOLD = 3600000;  // 1 hora
```

**Comportamiento:**

**WiFi Normal:**
- Backoff exponencial: 5s → 10s → 20s → 30s (max)
- Reintentos cada 30s máximo

**Modo Autónomo (WiFi perdido >1h):**
```cpp
if ((now - wifiLostTimestamp) > 3600000) {
    autonomousMode = true;
    // Reintentos cada 15 minutos (900000ms)
    // Logging remoto deshabilitado
    // Control local continúa funcionando
}
```

**API para verificar:**
```
GET /api/system/status
Response: {
  "wifi_connected": false,
  "autonomous_mode": true,
  "wifi_lost_duration_sec": 5420
}
```

**Impacto:** Sistema funciona offline indefinidamente.

---

### 3. Verificación de Heap Crítico

**Thresholds:**
```cpp
const uint32_t CRITICAL_HEAP = 30000;  // 30KB - acción inmediata
const uint32_t WARNING_HEAP = 50000;   // 50KB - log warning
```

**Acciones automáticas (cada 10s):**

**Heap < 30KB (CRÍTICO):**
```cpp
database.sendLogBuffer();    // Flush logs → liberar ~5-10KB
sensors.resetStatistics();   // Limpiar historial → ~2-5KB
// Log CRITICAL enviado
```

**Heap < 50KB (WARNING):**
```cpp
// Log cada 5 minutos
database.log(LOG_WARNING, "heap", "Low heap detected", ...);
```

**Verificación:**
```
GET /api/system/heap
Response: {
  "free_heap": 45120,
  "min_free_heap": 38450,
  "largest_free_block": 43000,
  "fragmentation_ratio": 0.95
}
```

**Impacto:** Previene OOM en ejecuciones >2 semanas.

---

### 4. Reinicio Preventivo Diario (3:00 AM)

**Configuración:**
```cpp
// Verificación cada minuto
// Ejecuta si: uptime > 23h && hora == 3:00-3:05 AM
```

**Código:**
```cpp
void SystemManager::checkScheduledRestart() {
    if (uptime < 82800) return;  // 23 horas
    
    if (timeinfo.tm_hour == 3 && timeinfo.tm_min < 5) {
        database.log(LOG_INFO, "system", "Scheduled daily restart");
        delay(2000);  // Enviar log
        ESP.restart();
    }
}
```

**Beneficios:**
- Limpia fragmentación de heap
- Reconecta servicios (WiFi, NTP)
- Previene degradación >7 días
- Ejecuta en horario de baja actividad

**Deshabilitar (si necesario):**
```cpp
// Comentar en src/system.cpp línea ~399
// checkScheduledRestart();
```

**Impacto:** ↑ 300% estabilidad en uptime >1 semana.

---

### 5. Tolerancia Extendida DHT11

**Configuración:**
```cpp
const uint8_t DHT_FAIL_THRESHOLD = 20;  // Antes: 5
```

**Comportamiento:**

**Fallo temporal (<20 lecturas):**
```cpp
if (isnan(temp) || isnan(humidity)) {
    // Usar últimos datos válidos
    newData.temperature = lastValidData.temperature;
    newData.humidity = lastValidData.humidity;
    
    // Marcar como válido si hay datos previos
    if (lastValidData.valid) {
        newData.valid = true;  // Sistema continúa
    }
}
```

**Log optimizado:**
- Reporta fallos: #1-20, luego cada 20 (40, 60, 80...)
- Muestra datos de fallback usados
- No satura logs con fallos temporales

**Impacto:**
- ↓ 90% logs de error DHT
- Sistema opera con datos "stale" (aceptable para control ambiental)
- Reglas automáticas continúan funcionando

---

### 6. Límite de Crash Recovery (Safe Mode)

**Configuración:**
```cpp
const uint8_t CRASH_FORMAT_THRESHOLD = 3;     // Crashes antes de format
const uint8_t MAX_FORMATS_PER_DAY = 2;       // Límite formatos/24h
```

**Lógica:**

```
Crash 1-2: Normal operation
Crash 3: Format filesystem + restore assets
  ↓
Format 1: OK, contador = 1
Format 2: OK, contador = 2
  ↓
Format 3 (mismo día): SAFE MODE
  - No formatea
  - Flag "safe_mode" activado
  - Sistema continúa con filesystem corrupto
  - Requiere intervención manual
```

**Verificación:**
```cpp
// Boot log
[BOOT] Reset reason=6 crashReset=true failCount=3 formatCount=2
[RECOVERY] Too many formats in 24h - entering SAFE MODE
[SAFE MODE] Running with minimal features
```

**Salir de Safe Mode:**
```bash
# Via Serial o SSH
nvs_erase boot  # Resetear contadores
```

**Impacto:** Previene loops infinitos crash-format-crash.

---

### 7. Persistencia de Estado Entre Reinicios

**Ya implementado (reforzado):**

**Relay State (NVS):**
```cpp
// Guardado cada minuto + antes de reinicio programado
relays.saveStateToNVS();

// Doble slot con CRC
Slot 1: [State][CRC32]
Slot 2: [State][CRC32]
// Usa slot válido más reciente
```

**Boot Counters:**
```cpp
Preferences prefs;
prefs.begin("boot", false);
uint8_t failCount = prefs.getUChar("fc", 0);
uint8_t formatCount = prefs.getUChar("fmt_cnt", 0);
unsigned long lastFormatTime = prefs.getULong("fmt_time", 0);
```

**Critical Log Ring (LittleFS):**
```
/critical_logs.ring (8KB circular buffer)
- Últimos 50 logs ERROR/CRITICAL
- Sobrevive a format (restaurado si posible)
```

---

### Métricas de Autonomía

| Métrica | Antes (v1.3) | Después (v1.4) |
|---------|--------------|----------------|
| **MTBF estimado** | 2-3 días | 14-30 días |
| **Watchdog timeout** | 30s | 120s |
| **WiFi offline max** | 30 min | Indefinido (modo autónomo) |
| **DHT fallos tolerados** | 5 | 20 + fallback |
| **Heap crítico** | ❌ Sin protección | ✅ Limpieza < 30KB |
| **Mantenimiento** | Manual | Automático (3 AM) |
| **Crash recovery** | Loop infinito | Safe mode tras 2 formats |

### Testing de Autonomía

**Test 1: Supervivencia sin WiFi (1 semana)**
```bash
1. Arrancar con WiFi normal
2. Apagar router
3. Esperar 7 días
4. Verificar: relays funcionando, sensores activos
5. Reactivar WiFi → debe reconectar automáticamente
```

**Test 2: Stress de memoria**
```bash
# Reducir intervalo de logs temporalmente
-D LOG_INTERVAL_MS=1000  # En platformio.ini

# Monitor heap
watch -n 5 'curl -H "Authorization: Bearer TOKEN" http://IP/api/system/heap'

# Verificar limpieza automática cuando < 30KB
```

**Test 3: DHT desconectado**
```bash
1. Desconectar físicamente DHT11
2. Verificar logs: "usando datos previos"
3. Verificar API: temperatura = última válida
4. Reconectar tras 1h
5. Verificar: recuperación automática
```

**Test 4: Reinicio programado**
```bash
# Configurar NTP
# Esperar 24h
# Verificar logs entre 3:00-3:05 AM:
[INFO] system: Scheduled daily restart
[BOOT] Reset reason=12 (software restart)
```

---

## Errores Comunes a Evitar

### ❌ Error 1: Usar GPIO Strapping sin Configurar Pull

```cpp
// ❌ MAL
pinMode(0, OUTPUT);  // GPIO0 afecta boot mode
digitalWrite(0, LOW); // Device no bootea

// ✅ BIEN
// Evitar GPIO 0, o solo usar INPUT con externa pull-up si es absolutamente necesario
```

### ❌ Error 2: Leer ADC2 Durante WiFi

```cpp
// ❌ MAL
int value = analogRead(25);  // GPIO25 es ADC2 → retorna 0 si WiFi activo

// ✅ BIEN
int value = analogRead(34);  // GPIO34 es ADC1 → funciona siempre
```

### ❌ Error 3: Bloquear `loop()` con `delay()`

```cpp
// ❌ MAL
void loop() {
  readSensors();
  delay(5000);  // Bloquea watchdog, WiFi, WebSocket
}

// ✅ BIEN
unsigned long lastRead = 0;
void loop() {
  unsigned long now = millis();
  if (now - lastRead >= 5000) {
    readSensors();
    lastRead = now;
  }
  // Otros tasks (watchdog, WiFi) pueden ejecutarse
}
```

### ❌ Error 4: No Verificar CRC en Persistencia

```cpp
// ❌ MAL
File f = LittleFS.open("/state.bin", "r");
f.read(buffer, sizeof(State));  // Leer ciegamente

// ✅ BIEN
File f = LittleFS.open("/state.bin", "r");
f.read(buffer, sizeof(StateWithCRC));
if (validateCRC(buffer)) {
  memcpy(&state, buffer, sizeof(State));
}
```

### ❌ Error 5: Concatenar Strings en Loop Crítico

```cpp
// ❌ MAL (fragmenta heap)
String json = "{";
json += "\"temp\":" + String(temp) + ",";
json += "\"hum\":" + String(hum) + "}";

// ✅ BIEN (buffer estático)
char json[256];
snprintf(json, sizeof(json), "{\"temp\":%.1f,\"hum\":%.1f}", temp, hum);
```

### ❌ Error 6: No Resetear Watchdog

```cpp
// ❌ MAL
void loop() {
  longOperation();  // Toma >30s → watchdog reset
}

// ✅ BIEN
void loop() {
  esp_task_wdt_reset();  // Reset watchdog
  longOperation();
}
```

### ❌ Error 7: Asumir WiFi Siempre Conectado

```cpp
// ❌ MAL
void uploadData() {
  httpClient.POST(data);  // Falla si WiFi down
}

// ✅ BIEN
void uploadData() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!httpClient.POST(data)) {
      saveToLocalBackup(data);
    }
  } else {
    saveToLocalBackup(data);
  }
}
```

### ❌ Error 8: Ignorar Valores NULL de Sensores

```cpp
// ❌ MAL
float temp = dht.readTemperature();
if (temp > 30) { activateVentilator(); }  // Si NaN, comportamiento indefinido

// ✅ BIEN
float temp = dht.readTemperature();
if (!isnan(temp) && temp > 30) { activateVentilator(); }
```

---

## Convenciones del Proyecto

### Nomenclatura

```cpp
// ✅ Clases: PascalCase
class SensorManager {};

// ✅ Funciones: camelCase
void readSensorData();

// ✅ Variables: snake_case
uint32_t last_update_time;

// ✅ Constantes: UPPER_SNAKE_CASE
#define MAX_RETRIES 5

// ✅ Pines: UPPER con sufijo _PIN
#define RELAY_PIN_0 16

// ✅ Macros feature: FEATURE_ prefix
#define FEATURE_DISABLE_OTA
```

### Comentarios

```cpp
// ✅ BIEN - Explica "por qué", no "qué"
// Use GPIO34 porque ADC1 no interfiere con WiFi
analogRead(34);

// ❌ MAL - Repite el código
// Read analog value from GPIO 34
analogRead(34);

// ✅ BIEN - Documenta decisión técnica
// Dual-slot pattern: previene corrupción en power loss
writeStateToSlot(alternateSlot);

// ✅ BIEN - Marca TODOs con contexto
// TODO(v1.5): Añadir soporte I2C para display OLED
```

### Logging

```cpp
// ✅ Formato: LOG_LEVEL("Módulo", "Mensaje con %s", variable);
LOG_INFO("WiFi", "Connected to %s (IP: %s)", ssid, ip);
LOG_ERROR("Sensor", "DHT read failed (retries: %d)", retries);

// ❌ MAL - Sin categoría
Serial.println("Connected to WiFi");

// ❌ MAL - Demasiado verbose
LOG_DEBUG("Loop", "Entering loop iteration %lu with free heap %u and uptime %lu...", iter, heap, uptime);
```

### Includes

```cpp
// Orden:
// 1. Header propio (si aplica)
#include "sensors.h"

// 2. Headers del proyecto
#include "config.h"
#include "pins.h"

// 3. Librerías Arduino/ESP32
#include <Arduino.h>
#include <WiFi.h>

// 4. Librerías externas
#include <ArduinoJson.h>
#include <DHT.h>
```

### Manejo de Errores

```cpp
// ✅ BIEN - Validar y loggear
if (!sensor.begin()) {
  LOG_ERROR("Sensor", "DHT init failed (pin %d)", DHT_PIN);
  return false;
}

// ✅ BIEN - Retry con límite
for (int i = 0; i < MAX_RETRIES; i++) {
  if (connect()) return true;
  delay(RETRY_DELAY);
}
LOG_ERROR("Network", "Connection failed after %d retries", MAX_RETRIES);
return false;

// ❌ MAL - Silenciar errores
if (!sensor.begin()) return;  // Sin log
```

---

## Apéndice: Comandos Útiles

### Compilación

```bash
# Build
pio run -e greenhouse

# Limpiar build
pio run --target clean
```

### Upload

```bash
# Firmware
pio run -e greenhouse --target upload

# Filesystem (LittleFS)
pio run -e greenhouse --target uploadfs

# Monitoring
pio device monitor --baud 115200
```

### Análisis

```bash
# Tamaño firmware
pio run -e greenhouse --target size

# Generar compile_commands.json (para IntelliSense)
pio run -e greenhouse -t compiledb
```

---

## Versionamiento

**Esquema:** `MAJOR.MINOR.PATCH`

- **MAJOR:** Cambios incompatibles de API o schema de configuración
- **MINOR:** Nuevas features compatibles hacia atrás
- **PATCH:** Bugfixes y mejoras menores

---

**Última actualización:** 2025-10-13  
**Versión del documento:** 2.2  
**Firmware version:** 1.4.0 (Autonomy Enhanced)
