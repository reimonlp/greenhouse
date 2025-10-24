// secrets.example.h
// Copiá este archivo como secrets.h y ajustá los valores. NO lo subas al repo.
// ADVERTENCIA: NO expongas credenciales reales en repos públicos.

#pragma once

// WiFi
#define WIFI_SSID        "TuSSID"
#define WIFI_PASSWORD    "TuPasswordWiFi"

// Device authentication for VPS/WebSocket (change in production!)
#define DEVICE_AUTH_TOKEN "esp32_greenhouse_secure_token_2024_change_this"

// MongoDB Atlas URI (SRV). Ejemplo provisto por el usuario:
// FORMATO: mongodb+srv://usuario:password@cluster0.zomntcg.mongodb.net/?retryWrites=true&w=majority&appName=Cluster0
#define MONGODB_URI      "mongodb+srv://usuario:password@cluster0.zomntcg.mongodb.net/?retryWrites=true&w=majority&appName=Cluster0"

// Parámetros para MongoDB Atlas Data API (opcional). Activa -DUSE_MONGODB_DATA_API en platformio.ini para usarla.
#define MONGODB_DATA_API_KEY   "REPLACE_WITH_DATA_API_KEY"        // x-api-key
#define MONGODB_APP_ID         "data-xxxxxxxxxxxx"                // App ID (ej: data-abcde-fghij)
#define MONGODB_DATA_SOURCE    "Cluster0"                         // Nombre del Data Source (usualmente el cluster)
#define MONGODB_DB_NAME        "invernadero"                     // Base de datos
#define MONGODB_COLL_SENSORS   "sensor_data"
#define MONGODB_COLL_LOGS      "system_logs"
#define MONGODB_COLL_STATS     "statistics"

// OTA
#define OTA_PASSWORD     "ota_password_123"

// Otros parámetros sensibles futuros pueden ir aquí.

