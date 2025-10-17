#ifndef VPS_WEBSOCKET_H
#define VPS_WEBSOCKET_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "secrets.h"      // MUST be included BEFORE vps_config.h for DEVICE_AUTH_TOKEN
#include "vps_config.h"

// Callback types
typedef void (*RelayCommandCallback)(int relayId, bool state);
typedef void (*SensorRequestCallback)();

// Connection metrics structure
struct ConnectionMetrics {
    unsigned long totalConnections;
    unsigned long authFailures;
    unsigned long reconnections;
    unsigned long messagesReceived;
    unsigned long messagesSent;
    unsigned long uptimeSeconds;
    unsigned long lastConnectionTime;
    unsigned long totalDisconnections;
};

class VPSWebSocketClient {
public:
    VPSWebSocketClient();
    
    // Connection management
    bool begin();
    void loop();
    bool isConnected();
    
    // Send data to server
    bool sendSensorData(float temperature, float humidity, float soilMoisture = -1, int tempErrors = 0, int humidityErrors = 0);
    bool sendRelayState(int relayId, bool state, const char* mode = "manual", const char* changedBy = "esp32");
    bool sendLog(const char* level, const char* message);
    bool sendMetrics(const ConnectionMetrics& metrics);
    
    // Set callbacks for incoming commands
    void onRelayCommand(RelayCommandCallback callback);
    void onSensorRequest(SensorRequestCallback callback);
    
    // Get connection status
    String getStatus();
    
    // Get connection metrics
    ConnectionMetrics getMetrics();

private:
    WebSocketsClient _webSocket;
    bool _connected;
    unsigned long _lastReconnectAttempt;
    unsigned long _lastPing;
    unsigned long _lastActivity;  // Track last message sent/received for intelligent heartbeat
    
    // Authentication failure tracking
    bool _authFailed;
    int _authFailureCount;
    unsigned long _lastAuthAttempt;
    
    // Circuit breaker pattern
    int _consecutiveFailures;
    bool _circuitBreakerOpen;
    unsigned long _circuitBreakerOpenTime;
    
    // Callbacks
    RelayCommandCallback _relayCommandCallback;
    SensorRequestCallback _sensorRequestCallback;
    
    // Connection metrics
    ConnectionMetrics _metrics;
    unsigned long _startTime;
    
    // WebSocket event handler
    static void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
    static VPSWebSocketClient* _instance;
    
    // Internal handlers
    void handleConnected();
    void handleDisconnected();
    void handleMessage(uint8_t * payload, size_t length);
    void handleRelayCommand(JsonObject& data);
    void handleSensorRequest();
    
    // Helper methods
    void sendEvent(const char* event, JsonDocument& data);
    bool reconnect();
};

#endif // VPS_WEBSOCKET_H
