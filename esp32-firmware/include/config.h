/**
 * @file config.h
 * @brief Central configuration file for ESP32 Greenhouse System
 * 
 * This file contains all compile-time configuration constants, timing values,
 * thresholds, and system parameters. It serves as the single source of truth
 * for system behavior and eliminates magic numbers throughout the codebase.
 * 
 * Organization:
 * - Debug/Logging configuration
 * - WiFi and network settings
 * - Sensor validation parameters
 * - Timing and interval constants
 * - Safety and reliability settings
 * - Feature flags and build options
 * 
 * All constants are documented with their units, valid ranges, and purpose.
 * Changes here affect the entire system's behavior and should be tested thoroughly.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#if __has_include(<Arduino.h>)
    #include <Arduino.h>
#else
    // Minimal stand-ins for Arduino types when building native tests
    using String = std::string;
#endif

// Incluir credenciales externas obligatorias (secrets.h debe existir)
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "secrets.h not found! Copy secrets.example.h to secrets.h and configure your credentials."
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
    // Use printf() for ESP32-C3 USB JTAG console compatibility
    #define DEBUG_PRINT(...) printf(__VA_ARGS__); fflush(stdout)
    #define DEBUG_PRINTLN(...) printf(__VA_ARGS__); printf("\n"); fflush(stdout)
    #define DEBUG_PRINTF(...) printf(__VA_ARGS__); fflush(stdout)
#endif

// ========== CONFIGURACIÓN DE HARDWARE ==========
// Mapeo de pines separado en pins.h para facilitar variantes
#include "pins.h"

// ========== CONFIGURACIÓN DE SISTEMA ==========
// WiFi credentials se definen solo en secrets.h
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RETRY_BASE_MS      5000


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

// ========== VALIDACIÓN DE RANGOS DHT11 ==========
// DHT11 datasheet specifications
#define DHT11_MIN_TEMP          0.0f    // DHT11 minimum temperature (°C)
#define DHT11_MAX_TEMP          50.0f   // DHT11 maximum temperature (°C)
#define DHT11_MIN_HUMIDITY      20.0f   // DHT11 minimum humidity (%)
#define DHT11_MAX_HUMIDITY      90.0f   // DHT11 maximum humidity (%)

// Anomaly detection thresholds
#define MAX_TEMP_CHANGE_PER_READ    10.0f   // Maximum °C change between consecutive reads
#define MAX_HUMIDITY_CHANGE_PER_READ 20.0f  // Maximum % change between consecutive reads

// Error handling
#define SENSOR_MAX_CONSECUTIVE_ERRORS 3     // Max errors before marking sensor as faulty

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

// Calibración del sensor de humedad de suelo
#define SOIL_MOISTURE_DRY_VALUE    3000
#define SOIL_MOISTURE_WET_VALUE    1000

// ========== TIMEOUTS Y DELAYS ==========
// WiFi & Network
#define WIFI_CONNECT_DELAY_MS           500     // Delay between WiFi connection attempts
#define WIFI_FAILED_RESTART_DELAY_MS    5000    // Delay before restart on WiFi failure
#define NTP_SYNC_RETRY_DELAY_MS         500     // Delay between NTP sync attempts

// WebSocket Connection
#define WS_HEARTBEAT_PING_INTERVAL_MS   15000   // WebSocket ping interval
#define WS_HEARTBEAT_PONG_TIMEOUT_MS    3000    // WebSocket pong timeout
#define WS_RECONNECT_INTERVAL_MS        5000    // WebSocket reconnection interval
#define WS_PING_IDLE_THRESHOLD_MS       30000   // Send ping if no activity for 30s
#define WS_REGISTRATION_TIMEOUT_MS      3000    // Device registration timeout
#define WS_REGISTRATION_DELAY_MS        100     // Delay before registration
#define WS_CONNECTION_CHECK_DELAY_MS    500     // Delay between connection checks
#define WS_INITIAL_STATE_DELAY_MS       2000    // Delay after sending initial states

// Authentication & Circuit Breaker
#define AUTH_BACKOFF_BASE_MS            30000   // Base delay for auth retry (30s)
#define AUTH_BACKOFF_MAX_MS             300000  // Max auth backoff delay (5 min)
#define AUTH_BACKOFF_JITTER_PERCENT     10      // ±10% jitter for backoff

// Health Checks & Monitoring
#define HEALTH_CHECK_INTERVAL_MS        60000   // Check VPS health every 60s
#define METRICS_SEND_INTERVAL_MS        300000  // Send metrics every 5 minutes
#define CIRCUIT_BREAKER_THRESHOLD       10      // Open circuit after 10 consecutive failures
#define CIRCUIT_BREAKER_TIMEOUT_MS      300000  // Circuit breaker timeout (5 minutes)
#define CIRCUIT_BREAKER_TEST_INTERVAL_MS 60000  // Test circuit every 1 minute
#define CIRCUIT_BREAKER_TEST_MOD_MS     1000    // Circuit breaker test modulo

// Sensor Reading
#define SENSOR_READ_MIN_INTERVAL_MS     2000    // Minimum interval between sensor reads
#define DHT_INIT_STABILIZE_DELAY_MS     2000    // DHT stabilization delay on init
#define SOIL_MOISTURE_READ_DELAY_MS     10      // Delay between soil moisture samples

// Relay Control
#define RELAY_STATE_SEND_DELAY_MS       100     // Delay between relay state transmissions

// System Startup
#define SYSTEM_STARTUP_DELAY_MS         1000    // Delay after serial init
#define LOOP_ITERATION_DELAY_MS         10      // Delay in main loop iteration


// ========== CONFIGURACIÓN DE WATCHDOG ==========
#define WATCHDOG_TIMEOUT_SEC    120

// ========== CONFIGURACIÓN OTA ==========
// OTA_PASSWORD se define solo en secrets.h
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
    LEVEL_DEBUG,
    LEVEL_INFO,
    LEVEL_WARNING,
    LEVEL_ERROR,
    LEVEL_CRITICAL
};

// ========== ESTRUCTURAS DE DATOS ==========
struct SensorData {
    float temperature;
    float humidity;
    float soil_moisture;
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
// FIRMWARE_VERSION se define solo en vps_config.h para evitar conflictos
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

// Logging detallado (activar en platformio.ini con -D VERBOSE_LOGGING)
// #define VERBOSE_LOGGING 1

// ========== CONTROL DE FEATURES / REDUCCIÓN ==========
// Nivel mínimo de log compilado (usa macros LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_CRITICAL)
#ifndef MIN_LOG_LEVEL
#define MIN_LOG_LEVEL LOG_INFO
#endif

// Desactivar fallback HTML embebido del dashboard y requerir archivos en LittleFS
#define FEATURE_NO_DASHBOARD_FALLBACK 1

// Desactivar OTA (reduce binario si no se usa actualización remota)
// #define FEATURE_DISABLE_OTA 1

// Desactivar envío remoto de base de datos
// #define FEATURE_DISABLE_REMOTE_DB 1

// Desactivar sensores de temperatura externos (DS18B20 en GPIO32/33) si no están conectados
// #define FEATURE_DISABLE_EXT_TEMPS 1

// Habilitar validación HMAC opcional en endpoints sensibles
// #define FEATURE_HMAC_AUTH 1

// Habilitar restauración automática de assets web embebidos
// Desactivar con -D ENABLE_EMBEDDED_ASSET_RESTORE=0 si no se desea incluir en firmware
#ifndef ENABLE_EMBEDDED_ASSET_RESTORE
#define ENABLE_EMBEDDED_ASSET_RESTORE 0  // Desactivado: simplificamos, sólo LittleFS sirve assets
#endif

// Control de política de sobrescritura:
// 0 = Nunca sobrescribir si el archivo existe
// 1 = Sobrescribir sólo si falta
// 2 = Sobrescribir en mismatch
#ifndef EMBEDDED_ASSETS_OVERWRITE_MODE
#define EMBEDDED_ASSETS_OVERWRITE_MODE 1
#endif


// Ajustar tiempo de estabilización DHT (por defecto 2000ms en begin()):
// DHT_STABILIZE_MS definido arriba en línea 138


#endif // CONFIG_H