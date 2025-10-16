// VPS API Configuration
// Este archivo reemplaza la configuración de MongoDB Data API
// para enviar datos directamente al VPS

#ifndef VPS_CONFIG_H
#define VPS_CONFIG_H

// ========== VPS API CONFIGURATION ==========
#define VPS_API_HOST        "168.181.185.42"
#define VPS_API_PORT        80
#define VPS_API_BASE_URL    "http://168.181.185.42"

// API Endpoints
#define VPS_ENDPOINT_SENSORS        "/api/sensors"
#define VPS_ENDPOINT_RELAY_STATE    "/api/relays/%d/state"  // %d = relay_id
#define VPS_ENDPOINT_RELAY_STATES   "/api/relays/states"
#define VPS_ENDPOINT_RULES          "/api/rules"
#define VPS_ENDPOINT_RULE_BY_ID     "/api/rules/%s"  // %s = rule_id
#define VPS_ENDPOINT_LOGS           "/api/logs"
#define VPS_ENDPOINT_HEALTH         "/health"

// Configuración de envío de datos
#define SENSOR_SEND_INTERVAL_MS     30000   // Enviar cada 30 segundos
#define RELAY_STATE_POLL_MS         5000    // Polling cada 5 segundos
#define RULES_SYNC_INTERVAL_MS      60000   // Sincronizar reglas cada 60 segundos
#define LOG_SEND_INTERVAL_MS        30000   // Enviar logs cada 30 segundos

// Retry configuration
#define HTTP_MAX_RETRIES            3
#define HTTP_RETRY_DELAY_MS         2000
#define HTTP_TIMEOUT_MS             10000

// Device identification
#define DEVICE_ID                   "ESP32_GREENHOUSE_01"

// ========== WEBSOCKET CONFIGURATION ==========
#define VPS_WEBSOCKET_HOST          "168.181.185.42"
#define VPS_WEBSOCKET_PORT          3000
#define VPS_WEBSOCKET_PATH          "/socket.io/?EIO=4&transport=websocket"

#endif // VPS_CONFIG_H
