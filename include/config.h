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
// Log levels: 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG
#ifndef LOG_LEVEL
    #define LOG_LEVEL 4  // Default: show all logs (DEBUG)
#endif

// Deshabilitar completamente Serial output en producción
#ifdef DISABLE_SERIAL_OUTPUT
    #define DEBUG_SERIAL_BEGIN(x) ((void)0)
    #define LOG_ERROR(...) ((void)0)
    #define LOG_WARN(...) ((void)0)
    #define LOG_INFO(...) ((void)0)
    #define LOG_DEBUG(...) ((void)0)
    #define LOG_ERRORF(...) ((void)0)
    #define LOG_WARNF(...) ((void)0)
    #define LOG_INFOF(...) ((void)0)
    #define LOG_DEBUGF(...) ((void)0)
    // Legacy macros for backward compatibility
    #define DEBUG_PRINT(...) ((void)0)
    #define DEBUG_PRINTLN(...) ((void)0)
    #define DEBUG_PRINTF(...) ((void)0)
#else
    #define DEBUG_SERIAL_BEGIN(x) Serial.begin(x)
    
    // Error level (always shown if LOG_LEVEL >= 1)
    #if LOG_LEVEL >= 1
        #define LOG_ERROR(...) Serial.print("❌ ERROR: "); Serial.println(__VA_ARGS__)
        #define LOG_ERRORF(...) Serial.print("❌ ERROR: "); Serial.printf(__VA_ARGS__)
    #else
        #define LOG_ERROR(...) ((void)0)
        #define LOG_ERRORF(...) ((void)0)
    #endif
    
    // Warn level (shown if LOG_LEVEL >= 2)
    #if LOG_LEVEL >= 2
        #define LOG_WARN(...) Serial.print("⚠ WARN: "); Serial.println(__VA_ARGS__)
        #define LOG_WARNF(...) Serial.print("⚠ WARN: "); Serial.printf(__VA_ARGS__)
    #else
        #define LOG_WARN(...) ((void)0)
        #define LOG_WARNF(...) ((void)0)
    #endif
    
    // Info level (shown if LOG_LEVEL >= 3)
    #if LOG_LEVEL >= 3
        #define LOG_INFO(...) Serial.print("ℹ INFO: "); Serial.println(__VA_ARGS__)
        #define LOG_INFOF(...) Serial.print("ℹ INFO: "); Serial.printf(__VA_ARGS__)
    #else
        #define LOG_INFO(...) ((void)0)
        #define LOG_INFOF(...) ((void)0)
    #endif
    
    // Debug level (shown if LOG_LEVEL >= 4)
    #if LOG_LEVEL >= 4
        #define LOG_DEBUG(...) Serial.println(__VA_ARGS__)
        #define LOG_DEBUGF(...) Serial.printf(__VA_ARGS__)
    #else
        #define LOG_DEBUG(...) ((void)0)
        #define LOG_DEBUGF(...) ((void)0)
    #endif
    
    // Legacy macros - map to DEBUG level for backward compatibility
    #define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
    #define DEBUG_PRINTLN(...) LOG_DEBUG(__VA_ARGS__)
    #define DEBUG_PRINTF(...) LOG_DEBUGF(__VA_ARGS__)
#endif

// ========== CONFIGURACIÓN DE HARDWARE ==========
// Mapeo de pines separado en pins.h para facilitar variantes
#include "pins.h"

// ========== CONFIGURACIÓN DE SISTEMA ==========
// WiFi credentials are now defined ONLY in secrets.h
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RETRY_BASE_MS      5000

// API y seguridad
// API_TOKEN is now defined ONLY in secrets.h
#define API_PORT            80
#define MAX_API_REQUESTS    100  // Por minuto

// NTP y tiempo
#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC      -10800
#define DAYLIGHT_OFFSET_SEC 0

// ========== LÍMITES DE SEGURIDAD ==========
#define MAX_TEMP_CELSIUS        35.0f
#define MIN_TEMP_CELSIUS        5.0f
#define MAX_HUMIDITY_PERCENT    95.0f
#define MIN_HUMIDITY_PERCENT    20.0f
#define MAX_IRRIGATION_TIME_MS  300000  // 5 minutos máximo
#define MAX_HEATING_TIME_MS     1800000 // 30 minutos máximo

// ========== CONFIGURACIÓN DE SENSORES ==========
#define DHT_TYPE                DHT11
#define SENSOR_READ_INTERVAL_MS 5000
#define SOIL_MOISTURE_SAMPLES   10
#ifndef SOIL_SAMPLE_INTERVAL_MS
#define SOIL_SAMPLE_INTERVAL_MS 10
#endif

// ========== CONFIGURACIÓN DE MÉTRICAS ==========
#ifndef LOOP_EMA_ALPHA
#define LOOP_EMA_ALPHA 0.05f
#endif
#define TEMP_SENSOR_PRECISION   12
#ifndef DHT_STABILIZE_MS
#define DHT_STABILIZE_MS 2000
#endif
#ifndef DHT_STABILIZE_MIN_MS
#define DHT_STABILIZE_MIN_MS 600
#endif
#ifndef DHT_STABILIZE_DECAY_FACTOR
#define DHT_STABILIZE_DECAY_FACTOR 0.5f
#endif

// Calibración de sensores de humedad de suelo
#define SOIL_MOISTURE_DRY_VALUE    3000
#define SOIL_MOISTURE_WET_VALUE    1000

// ========== CONFIGURACIÓN DE WATCHDOG ==========
#define WATCHDOG_TIMEOUT_SEC    120

// ========== CONFIGURACIÓN OTA ==========
// OTA_PASSWORD is now defined only in secrets.h
#define OTA_PORT                3232

// ========== CONFIGURACIÓN DE ALERTAS ==========
#define LED_BLINK_FAST_MS       250
#define LED_BLINK_SLOW_MS       1000
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
// FIRMWARE_VERSION is now defined ONLY in vps_config.h to avoid conflicts
// Do NOT define it here - vps_config.h is the single source of truth
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