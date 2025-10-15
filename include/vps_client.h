#ifndef VPS_CLIENT_H
#define VPS_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "vps_config.h"

class VPSClient {
public:
    VPSClient();
    
    // Sensor data
    bool sendSensorData(float temperature, float humidity, float soilMoisture = -1);
    
    // Relay states
    bool sendRelayState(int relayId, bool state, const char* mode = "manual", const char* changedBy = "esp32");
    bool getRelayStates(bool states[4]);
    
    // Rules
    bool syncRules();
    String getRules(int relayId = -1);
    bool createRule(int relayId, const char* sensor, const char* op, float threshold, const char* action);
    bool deleteRule(const String& ruleId);
    
    // Logs
    bool sendLog(const char* level, const char* message, const char* metadata = nullptr);
    
    // Health check
    bool healthCheck();
    
    // Get last error
    String getLastError() { return _lastError; }

private:
    HTTPClient _http;
    String _lastError;
    
    bool makeRequest(const char* method, const char* endpoint, const char* payload = nullptr, String* response = nullptr);
    String buildUrl(const char* endpoint);
    void setLastError(const String& error);
};

#endif // VPS_CLIENT_H
