// Configuración principal para WebSocket y device info

#ifndef VPS_CONFIG_H
#define VPS_CONFIG_H

// ...existing code...

// Device identification
#define DEVICE_ID                   "ESP32_GREENHOUSE_01"
// FIRMWARE_VERSION: Single source of truth (NOT defined in config.h)
#define FIRMWARE_VERSION            "2.3-ota"

// Security - Authentication token
// Debe definirse en secrets.h
#ifndef DEVICE_AUTH_TOKEN
#error "DEVICE_AUTH_TOKEN must be defined in secrets.h - this is a security requirement. Copy secrets.example.h to secrets.h and configure your authentication token."
#endif

// ========== CONFIGURACIÓN DE WEBSOCKET ==========
#define VPS_WEBSOCKET_HOST          "reimon.dev"
#define VPS_WEBSOCKET_PORT          443
#define VPS_WEBSOCKET_PATH          "/greenhouse/socket.io/?EIO=4&transport=websocket"
#define VPS_WEBSOCKET_USE_SSL       true

#endif // VPS_CONFIG_H
