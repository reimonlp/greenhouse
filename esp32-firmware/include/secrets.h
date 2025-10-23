// secrets.h
// Archivo generado automáticamente para pruebas. Cambia los valores en producción.
#pragma once

// WiFi
#define WIFI_SSID        "TuSSID"
#define WIFI_PASSWORD    "TuPasswordWiFi"

// API token (>=24 chars aleatorios recomendados)
#define API_TOKEN        "token_super_secreto"

// Device authentication for VPS/WebSocket (change in production!)
#define DEVICE_AUTH_TOKEN "esp32_greenhouse_secure_token_2024_change_this"

// MongoDB Atlas URI (SRV)
#define MONGODB_URI      "mongodb+srv://usuario:password@cluster0.zomntcg.mongodb.net/?retryWrites=true&w=majority&appName=Cluster0"

// Parámetros para MongoDB Atlas Data API (opcional)
#define MONGODB_DATA_API_KEY   "REPLACE_WITH_DATA_API_KEY"
#define MONGODB_APP_ID         "data-xxxxxxxxxxxx"
#define MONGODB_DATA_SOURCE    "Cluster0"
#define MONGODB_DB_NAME        "invernadero"
#define MONGODB_COLL_SENSORS   "sensor_data"
#define MONGODB_COLL_LOGS      "system_logs"
#define MONGODB_COLL_STATS     "statistics"

// OTA
#define OTA_PASSWORD     "ota_password_123"
