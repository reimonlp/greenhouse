#ifndef VPS_CLIENT_H
#define VPS_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "secrets.h"      // MUST be included BEFORE vps_config.h for DEVICE_AUTH_TOKEN
#include "vps_config.h"

class VPSClient {
public:
    VPSClient();
    
    void begin();
    
    // Sensor data
    bool sendSensorData(float temperature, float humidity, float soilMoisture = -1);
    
    // Health check
    bool healthCheck();
    
    // Legacy HTTP API - unused (WebSocket preferred)
    /*
    bool sendRelayState(int relayId, bool state, const char* mode = "manual", const char* changedBy = "esp32");
    bool getRelayStates(bool states[4]);
    String getRules(int relayId = -1);
    bool createRule(int relayId, const char* sensor, const char* op, float threshold, const char* action);
    bool deleteRule(const String& ruleId);
    bool sendLog(const char* level, const char* message, const char* metadata = nullptr);
    bool syncRules();
    */
    
    // Get last error
    String getLastError() { return _lastError; }

private:
    HTTPClient _http;
    WiFiClientSecure _wifiClient;
    String _lastError;
    bool _isConnected;
    
    int makeRequest(const String& endpoint, const String& method, const String& payload, String& response);
    String buildUrl(const char* endpoint);
    void setLastError(const String& error);
};

#endif // VPS_CLIENT_H
