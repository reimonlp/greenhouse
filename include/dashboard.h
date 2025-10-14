#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "config.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

class DashboardManager {
private:
    AsyncWebServer* server;
    AsyncWebSocket* ws = nullptr; // WebSocket server
    // Helper to serve a static file with optional gzip (pre-compressed .gz in LittleFS)
    void serveStaticCompressed(AsyncWebServerRequest* req, const char* path, const char* contentType, bool allowFallback = false);
    
public:
    DashboardManager();
    ~DashboardManager();
    
    bool begin();
    void setupRoutes(AsyncWebServer* webServer);
    void sendSensorUpdate(const SensorData& data);
    void sendSystemHeartbeat();
    void broadcastMessage(const String& type, const String& data);
    String formatUptime(unsigned long uptime);
    void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    
private:
    // Fallback y templates eliminados (FEATURE_NO_DASHBOARD_FALLBACK)
};

extern DashboardManager dashboard;

#endif // DASHBOARD_H