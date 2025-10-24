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
        // Sensor data por WebSocket
        bool sendSensorData(float temperature, float humidity, float soilMoisture = -1);
    
    // Health check
    
    // Legacy HTTP API - unused (WebSocket preferred)
    /*
    */
    
    // Get last error
    String getLastError() { return _lastError; }

private:
    String _lastError;
    bool _isConnected;
    
};

#endif // VPS_CLIENT_H
