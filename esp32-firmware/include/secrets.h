// secrets.h
// Archivo generado automáticamente para pruebas. Cambia los valores en producción.
#pragma once

// WiFi
#define WIFI_SSID        "FDC"
#define WIFI_PASSWORD    "unacagada"

// API token (>=24 chars aleatorios recomendados)
#define API_TOKEN        "GHAPI_2f8c1e7a4b9d4e6f8c2a7e1b3c4d5f6a"

// Device authentication for VPS/WebSocket (change in production!)
#define DEVICE_AUTH_TOKEN "GHDEV_7e4b2c9a1f6d3e8b5c2a9f7e1d4c6b8a"

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
#define OTA_PASSWORD     "GHOTA_!vP9rT7sQ2wL6zB8"
