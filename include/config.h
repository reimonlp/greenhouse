#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#if __has_include(<Arduino.h>)
    #include <Arduino.h>
#else
    // Minimal stand-ins for Arduino types when building native tests
    using String = std::string;
#endif

// Incluir credenciales externas opcionales (no versionar secrets.h real)
#if __has_include("secrets.h")
#include "secrets.h"
#endif

// ========== MACROS DE DEBUG ==========
// Deshabilitar completamente Serial output en producción
#ifdef DISABLE_SERIAL_OUTPUT
    #define DEBUG_SERIAL_BEGIN(x) ((void)0)
    #define DEBUG_PRINT(...) ((void)0)
    #define DEBUG_PRINTLN(...) ((void)0)
    #define DEBUG_PRINTF(...) ((void)0)
#else
    #define DEBUG_SERIAL_BEGIN(x) Serial.begin(x)
    #define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
    #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
    #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#endif

// ========== CONFIGURACIÓN DE HARDWARE ==========
// Mapeo de pines separado en pins.h para facilitar variantes
#include "pins.h"

// ========== CONFIGURACIÓN DE SISTEMA ==========
// WiFi conexión fija (se elimina WiFiManager). Credenciales objetivo.
#define WIFI_SSID           "FDC"
#define WIFI_PASSWORD       "unacagada"
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RETRY_BASE_MS      5000
// Optional initial delay before first WiFi.begin (define via build_flags if needed)
// #define WIFI_INITIAL_DELAY_MS 0

// Valores legacy (portal cautivo) eliminados:
// #define AP_SSID             "ESP32-Invernadero"
// #define AP_PASSWORD         "invernadero123"

// API y seguridad
#define API_PORT            80
#ifndef API_TOKEN
#define API_TOKEN           "tu_token_secreto_aqui"  // Cambiar en producción (overridden by secrets.h if defined there)
#endif
#define MAX_API_REQUESTS    100  // Por minuto

// NTP y tiempo
#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC      -10800  // GMT-3 (Argentina)
#define DAYLIGHT_OFFSET_SEC 0

// MongoDB y logging
#ifndef MONGODB_URI
#define MONGODB_URI         "mongodb+srv://usuario:password@cluster.mongodb.net/invernadero"
#endif

// Opcionales para Data API (definir en secrets.h)
#ifndef MONGODB_DATA_API_KEY
#define MONGODB_DATA_API_KEY ""  // define en secrets.h
#endif
#ifndef MONGODB_APP_ID
#define MONGODB_APP_ID ""  // p.ej. "data-xxxxx"
#endif
#ifndef MONGODB_DATA_SOURCE
#define MONGODB_DATA_SOURCE "Cluster0"
#endif
#ifndef MONGODB_DATA_API_BASE
// Si usás servidor local, definirlo en secrets.h como MONGODB_DATA_API_BASE_LOCAL
#ifdef MONGODB_DATA_API_BASE_LOCAL
#define MONGODB_DATA_API_BASE MONGODB_DATA_API_BASE_LOCAL
#else
#define MONGODB_DATA_API_BASE "https://data.mongodb-api.com" // fallback Atlas (si se usa)
#endif
#endif
#ifndef MONGODB_DB_NAME
#define MONGODB_DB_NAME "invernadero"
#endif
#ifndef MONGODB_COLL_SENSORS
#define MONGODB_COLL_SENSORS "sensor_data"
#endif
#ifndef MONGODB_COLL_LOGS
#define MONGODB_COLL_LOGS "system_logs"
#endif
#ifndef MONGODB_COLL_STATS
#define MONGODB_COLL_STATS "statistics"
#endif

#define LOG_BUFFER_SIZE     50    // Logs en buffer antes de enviar
#define LOG_INTERVAL_MS     30000 // Envío cada 30 segundos
// Parámetros de batching/jitter para logs (L3)
#ifndef LOG_MIN_BATCH
#define LOG_MIN_BATCH 5        // Enviar sólo si al menos N logs acumulados (salvo ERROR/CRITICAL)
#endif
#ifndef LOG_MAX_INTERVAL_MS
#define LOG_MAX_INTERVAL_MS 45000 // Forzar flush si pasa este tiempo sin enviar
#endif
#ifndef LOG_FLUSH_JITTER_PCT
#define LOG_FLUSH_JITTER_PCT 15 // +/- % de jitter aplicado a LOG_INTERVAL_MS
#endif

// ========== LÍMITES DE SEGURIDAD ==========
#define MAX_TEMP_CELSIUS        35.0f
#define MIN_TEMP_CELSIUS        5.0f
#define MAX_HUMIDITY_PERCENT    95.0f
#define MIN_HUMIDITY_PERCENT    20.0f
#define MAX_IRRIGATION_TIME_MS  300000  // 5 minutos máximo
#define MAX_HEATING_TIME_MS     1800000 // 30 minutos máximo

// ========== CONFIGURACIÓN DE SENSORES ==========
#define DHT_TYPE                DHT11   // Sensor DHT11 (solo soporte para DHT11)
#define SENSOR_READ_INTERVAL_MS 5000    // Leer sensores cada 5 segundos
#define SOIL_MOISTURE_SAMPLES   10      // Promedio de lecturas
#ifndef SOIL_SAMPLE_INTERVAL_MS
#define SOIL_SAMPLE_INTERVAL_MS 10      // Intervalo entre muestras no bloqueantes de suelo
#endif

// ========== CONFIGURACIÓN DE MÉTRICAS ==========
#ifndef LOOP_EMA_ALPHA
#define LOOP_EMA_ALPHA 0.05f
#endif
#define TEMP_SENSOR_PRECISION   12      // Bits de precisión para DS18B20
// Estabilización adaptativa DHT (S3)
#ifndef DHT_STABILIZE_MS
#define DHT_STABILIZE_MS 2000
#endif
#ifndef DHT_STABILIZE_MIN_MS
#define DHT_STABILIZE_MIN_MS 600  // mínimo tras calentamiento inicial
#endif
#ifndef DHT_STABILIZE_DECAY_FACTOR
#define DHT_STABILIZE_DECAY_FACTOR 0.5f  // reduce a la mitad tras lectura exitosa
#endif

// Calibración de sensores de humedad de suelo
#define SOIL_MOISTURE_DRY_VALUE    3000  // Valor ADC para suelo seco
#define SOIL_MOISTURE_WET_VALUE    1000  // Valor ADC para suelo húmedo

// ========== CONFIGURACIÓN DE WATCHDOG ==========
#define WATCHDOG_TIMEOUT_SEC    120  // 2 minutos para operaciones lentas (WiFi, sensores)

// ========== CONFIGURACIÓN OTA ==========
#define OTA_PASSWORD            "ota_password_123"  // Cambiar en producción
#define OTA_PORT                3232

// ========== CONFIGURACIÓN DE ALERTAS ==========
// Buzzer eliminado completamente
#define LED_BLINK_FAST_MS       250
#define LED_BLINK_SLOW_MS       1000
// Definir STATUS_LED_ACTIVE_LOW vía build_flags si el LED requiere LOW para encender.
#ifdef STATUS_LED_ACTIVE_LOW
#define LED_WRITE_ON(pin)  digitalWrite((pin), LOW)
#define LED_WRITE_OFF(pin) digitalWrite((pin), HIGH)
#else
#define LED_WRITE_ON(pin)  digitalWrite((pin), HIGH)
#define LED_WRITE_OFF(pin) digitalWrite((pin), LOW)
#endif

// ========== ESTADOS DEL SISTEMA ==========
enum SystemState {
    SYSTEM_INITIALIZING,
    SYSTEM_NORMAL,
    SYSTEM_PAUSED,
    SYSTEM_ERROR,
    SYSTEM_MAINTENANCE
};

enum RelayMode {
    RELAY_MODE_MANUAL,
    RELAY_MODE_AUTO
};

enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_CRITICAL
};

// ========== ESTRUCTURAS DE DATOS ==========
struct SensorData {
    float temperature;
    float humidity;
    float soil_moisture_1;
    float soil_moisture_2;
    // External temp sensors removed
    unsigned long timestamp;
    bool valid;
};

struct RelayState {
    bool is_on;
    RelayMode mode;
    unsigned long last_change;
    unsigned long total_on_time;
    String auto_rule;
};

struct SystemStats {
    float temp_min, temp_max, temp_avg;
    float humidity_min, humidity_max, humidity_avg;
    float soil_min, soil_max, soil_avg;
    unsigned long heating_time;
    unsigned long irrigation_time;
    unsigned long uptime;
    unsigned long last_reset_time;
};

// ========== VERSIÓN DEL FIRMWARE ==========
#define FIRMWARE_VERSION        "1.0.0"
#define BUILD_DATE              __DATE__ " " __TIME__
// Schema version for persisted & JSON config exports
#ifndef CONFIG_SCHEMA_VERSION
#define CONFIG_SCHEMA_VERSION 1
#endif

// ========== RATE LIMITER CONFIG ==========
#ifndef RATE_LIMIT_SLOTS
#define RATE_LIMIT_SLOTS 6
#endif

#ifndef VERBOSE_LOGS
#define VERBOSE_LOGS 0
#endif
#ifndef MAX_API_REQUESTS
#define MAX_API_REQUESTS 60
#endif

// Logging detallado (descomentar en platformio.ini con -D VERBOSE_LOGGING)
// #define VERBOSE_LOGGING 1

// ========== CONTROL DE FEATURES / REDUCCIÓN ==========
// Nivel mínimo de log compilado (LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_CRITICAL)
#ifndef MIN_LOG_LEVEL
#define MIN_LOG_LEVEL LOG_INFO
#endif

// Desactivar fallback HTML embebido del dashboard y requerir archivos en LittleFS
#define FEATURE_NO_DASHBOARD_FALLBACK 1

// Desactivar OTA (reduce binario si no se usa actualización remota)
// #define FEATURE_DISABLE_OTA 1

// Desactivar envío remoto de base de datos (sustituye antiguo ENABLE_REMOTE_DB)
// #define FEATURE_DISABLE_REMOTE_DB 1

// Desactivar sensores de temperatura externos (DS18B20 en GPIO32/33) si no están conectados
// Reduce código y evita logs de error de desconexión.
// #define FEATURE_DISABLE_EXT_TEMPS 1

// Habilitar validación HMAC opcional en endpoints sensibles (Authorization + X-Signature)
// #define FEATURE_HMAC_AUTH 1

// Habilitar restauración automática de assets web embebidos (index.html, style.css, script.js mínimos)
// Desactivar con -D ENABLE_EMBEDDED_ASSET_RESTORE=0 si no se desea incluir en firmware
#ifndef ENABLE_EMBEDDED_ASSET_RESTORE
#define ENABLE_EMBEDDED_ASSET_RESTORE 0  // Desactivado: simplificamos, sólo LittleFS sirve assets
#endif

// Control de política de sobrescritura:
// 0 = Nunca sobrescribir si el archivo existe (aunque difiera en tamaño)
// 1 = Sobrescribir sólo si falta (ignora mismatch)
// 2 = Sobrescribir en mismatch (comportamiento original)
#ifndef EMBEDDED_ASSETS_OVERWRITE_MODE
#define EMBEDDED_ASSETS_OVERWRITE_MODE 1
#endif

// Archivo para persistir hash de token/API (si se rota en runtime)
#ifndef TOKEN_HASH_FILE
#define TOKEN_HASH_FILE "/api_token.sha" // 32 bytes bin CRC-protected
#endif

// Ajustar tiempo de estabilización DHT (por defecto 2000ms en begin()):
#ifndef DHT_STABILIZE_MS
#define DHT_STABILIZE_MS 2000
#endif


#endif // CONFIG_H
// Build profile selection (defined via -D BUILD_PROFILE_MINIMAL / STANDARD / OBSERVABILITY)
#if defined(BUILD_PROFILE_MINIMAL)
#ifndef FEATURE_DISABLE_OTA
#define FEATURE_DISABLE_OTA 1
#endif
#ifndef FEATURE_DISABLE_REMOTE_DB
#define FEATURE_DISABLE_REMOTE_DB 1
#endif
#ifndef FEATURE_NO_DASHBOARD_FALLBACK
#define FEATURE_NO_DASHBOARD_FALLBACK 1
#endif
#undef MIN_LOG_LEVEL
#define MIN_LOG_LEVEL LOG_WARNING
#endif

#if defined(BUILD_PROFILE_STANDARD)
// Standard: defaults (keep current settings)
#endif

#if defined(BUILD_PROFILE_OBSERVABILITY)
// Observability: keep remote DB enabled, detailed debug logs optional
#undef MIN_LOG_LEVEL
#define MIN_LOG_LEVEL LOG_DEBUG
#endif