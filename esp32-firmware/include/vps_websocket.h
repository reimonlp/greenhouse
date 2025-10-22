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

/**
 * @struct ConnectionMetrics
 * @brief Tracks WebSocket connection statistics for monitoring
 */
struct ConnectionMetrics {
    unsigned long totalConnections;      ///< Total successful connections
    unsigned long authFailures;          ///< Authentication failure count
    unsigned long reconnections;         ///< Automatic reconnection attempts
    unsigned long messagesReceived;      ///< Incoming messages count
    unsigned long messagesSent;          ///< Outgoing messages count
    unsigned long uptimeSeconds;         ///< Total connection uptime
    unsigned long lastConnectionTime;    ///< Timestamp of last connection
    unsigned long totalDisconnections;   ///< Total disconnection events
};

/**
 * @class VPSWebSocketClient
 * @brief WebSocket client for real-time communication with VPS backend
 * 
 * Handles secure WebSocket communication with the greenhouse backend server.
 * Implements authentication, reconnection logic, circuit breaker pattern,
 * and real-time data transmission for sensors and relay control.
 * 
 * Key Features:
 * - SSL/TLS encrypted communication
 * - Automatic reconnection with exponential backoff
 * - Circuit breaker pattern for fault tolerance
 * - Authentication with token-based security
 * - Real-time sensor data transmission
 * - Remote relay control via WebSocket commands
 */
class VPSWebSocketClient {
public:
    VPSWebSocketClient();
    
    // Connection management
    bool begin();
    void loop();
    bool isConnected();
    
    // Send data to server
    /**
     * @brief Send sensor readings to backend server
     * @param temperature Temperature in Celsius
     * @param humidity Relative humidity percentage
     * @param soilMoisture Soil moisture percentage (-1 if not available)
     * @param tempErrors Consecutive temperature sensor errors
     * @param humidityErrors Consecutive humidity sensor errors
     * @return true if data sent successfully
     */
    bool sendSensorData(float temperature, float humidity, float soilMoisture = -1, int tempErrors = 0, int humidityErrors = 0);
    
    /**
     * @brief Send relay state change to backend
     * @param relayId Relay number (0-3)
     * @param state New relay state (true=on, false=off)
     * @param mode Control mode ("manual", "auto", "rule")
     * @param changedBy Who initiated the change
     * @return true if state sent successfully
     */
    bool sendRelayState(int relayId, bool state, const char* mode = "manual", const char* changedBy = "esp32");
    
    /**
     * @brief Send log message to backend for remote monitoring
     * @param level Log level ("DEBUG", "INFO", "WARN", "ERROR")
     * @param message Log message content
     * @return true if log sent successfully
     */
    bool sendLog(const char* level, const char* message);
    
    /**
     * @brief Send connection metrics to backend
     * @param metrics ConnectionMetrics struct with statistics
     * @return true if metrics sent successfully
     */
    bool sendMetrics(const ConnectionMetrics& metrics);
    
    // Set callbacks for incoming commands
    /**
     * @brief Register callback for remote relay control commands
     * @param callback Function to call when relay command received
     */
    void onRelayCommand(RelayCommandCallback callback);
    
    /**
     * @brief Register callback for sensor data requests
     * @param callback Function to call when sensor request received
     */
    void onSensorRequest(SensorRequestCallback callback);
    
    // Get connection status
    /**
     * @brief Get human-readable connection status
     * @return String describing current connection state
     */
    String getStatus();
    
    /**
     * @brief Get current connection metrics
     * @return ConnectionMetrics struct with current statistics
     */
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
