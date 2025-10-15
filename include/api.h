#ifndef API_H
#define API_H

#include "config.h"
#include "sensors.h"
#include "relays.h"
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <IPAddress.h>

class APIManager {
private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;
    String authToken;
    unsigned long requestCount;
    unsigned long lastResetTime;
    uint8_t authTokenHash[32];
    bool tokenHashed = false;
    bool tokenDirty = false; // needs persistence
    // Body buffer for JSON restore endpoint
    static const size_t RESTORE_BUF_MAX = 1536;
    char restoreBuf[RESTORE_BUF_MAX];
    size_t restoreLen = 0;
    // (Legacy storage removed; now using pure RateLimiter module)
    
public:
    APIManager();
    ~APIManager();
    
    bool begin();
    void handleClient();
    
    // Configuración de autenticación
    void setAuthToken(const String& token);
    bool validateToken(AsyncWebServerRequest* request);
    void computeTokenHash();
    bool persistTokenHash();
    bool loadPersistedTokenHash();
    bool rotateToken(const String& current, const String& replacement);
    
    // Rate limiting
    bool checkRateLimit(AsyncWebServerRequest* request);
    void resetRateLimit();
    
    // WebSocket broadcasting
    void broadcastSensorData();
    void broadcastRelayState(int relayId);
    void broadcastSystemStatus();
    void broadcastRuleEvent(int relayId, const String& ruleName, const String& action);
    void sendToClient(uint32_t clientId, const String& message);
    
private:
    // Handlers de endpoints
    void setupRoutes();
    void setupWebSocket();
    void handleWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                             AwsEventType type, void *arg, uint8_t *data, size_t len);
    void handleWebSocketMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len);
    
    // Endpoints de sensores
    void handleGetSensors(AsyncWebServerRequest* request);
    void handleGetSensorHistory(AsyncWebServerRequest* request);
    
    // Endpoints de relays
    void handleGetRelays(AsyncWebServerRequest* request);
    void handleSetRelay(AsyncWebServerRequest* request);
    void handleSetRelayMode(AsyncWebServerRequest* request);
    void handleSetAutoRule(AsyncWebServerRequest* request);
    
    // Endpoints de reglas
    void handleGetRelayRules(AsyncWebServerRequest* request);
    void handleAddRelayRule(AsyncWebServerRequest* request);
    void handleUpdateRelayRule(AsyncWebServerRequest* request);
    void handleDeleteRelayRule(AsyncWebServerRequest* request);
    void handleClearRelayRules(AsyncWebServerRequest* request);
    
    // Endpoints de sistema
    void handleGetSystemStatus(AsyncWebServerRequest* request);
    void handleGetStatistics(AsyncWebServerRequest* request);
    void handleSystemPause(AsyncWebServerRequest* request);
    void handleSystemReset(AsyncWebServerRequest* request);
    void handleWiFiReset(AsyncWebServerRequest* request);
    void handleGetUptime(AsyncWebServerRequest* request);
    
    // Endpoints de configuración
    void handleGetConfig(AsyncWebServerRequest* request);
    void handleSetConfig(AsyncWebServerRequest* request);
    void handleBackupConfig(AsyncWebServerRequest* request);
    void handleRestoreConfig(AsyncWebServerRequest* request);
    void handleCalibrateSensor(AsyncWebServerRequest* request);
    
    // Endpoints de logs
    void handleGetLogs(AsyncWebServerRequest* request);
    void handleClearLogs(AsyncWebServerRequest* request);
    void handleRotateToken(AsyncWebServerRequest* request);
    
    // OTA y actualizaciones
    void handleOTAUpdate(AsyncWebServerRequest* request);
    void handleGetFirmwareInfo(AsyncWebServerRequest* request);
    
    // Utilities
    void sendJSONResponse(AsyncWebServerRequest* request, int code, const String& json);
    void sendErrorResponse(AsyncWebServerRequest* request, int code, const String& error);
    String createJSONError(const String& error);
    
    // CORS y headers
    void setCORSHeaders(AsyncWebServerResponse* response);
    
    // Body handling
    String getRequestBody(AsyncWebServerRequest* request);
    
    String lastError;

    void appendBodyChunk(AsyncWebServerRequest* request, uint8_t *data, size_t len, size_t index, size_t total);
};

extern APIManager api;

#endif // API_H