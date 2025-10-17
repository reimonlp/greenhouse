#include "vps_websocket.h"
#include "config.h"

VPSWebSocketClient* VPSWebSocketClient::_instance = nullptr;

VPSWebSocketClient::VPSWebSocketClient() {
    _connected = false;
    _lastReconnectAttempt = 0;
    _lastPing = 0;
    _lastActivity = 0;
    _authFailed = false;
    _authFailureCount = 0;
    _lastAuthAttempt = 0;
    _consecutiveFailures = 0;
    _circuitBreakerOpen = false;
    _circuitBreakerOpenTime = 0;
    _relayCommandCallback = nullptr;
    _sensorRequestCallback = nullptr;
    _instance = this;
    _startTime = millis();
    
    // Initialize metrics
    _metrics.totalConnections = 0;
    _metrics.authFailures = 0;
    _metrics.reconnections = 0;
    _metrics.messagesReceived = 0;
    _metrics.messagesSent = 0;
    _metrics.uptimeSeconds = 0;
    _metrics.lastConnectionTime = 0;
    _metrics.totalDisconnections = 0;
}

bool VPSWebSocketClient::begin() {
    DEBUG_PRINTLN("Initializing WebSocket connection...");
    
    #ifdef VPS_WEBSOCKET_USE_SSL
    _webSocket.beginSSL(VPS_WEBSOCKET_HOST, VPS_WEBSOCKET_PORT, VPS_WEBSOCKET_PATH);
    DEBUG_PRINTF("WebSocket SSL configured: wss://%s:%d%s\n", VPS_WEBSOCKET_HOST, VPS_WEBSOCKET_PORT, VPS_WEBSOCKET_PATH);
    #else
    _webSocket.begin(VPS_WEBSOCKET_HOST, VPS_WEBSOCKET_PORT, VPS_WEBSOCKET_PATH);
    DEBUG_PRINTF("WebSocket configured: ws://%s:%d%s\n", VPS_WEBSOCKET_HOST, VPS_WEBSOCKET_PORT, VPS_WEBSOCKET_PATH);
    #endif
    
    _webSocket.onEvent(webSocketEvent);
    // Disable automatic heartbeat - we'll send manual pings based on activity
    _webSocket.enableHeartbeat(WS_HEARTBEAT_PING_INTERVAL_MS, WS_HEARTBEAT_PONG_TIMEOUT_MS, 0);  // 0 = disable ping, keep pong handling
    _webSocket.setReconnectInterval(WS_RECONNECT_INTERVAL_MS);
    
    return true;
}

void VPSWebSocketClient::loop() {
    // Circuit breaker: stop trying if too many consecutive failures
    if (_circuitBreakerOpen) {
        unsigned long timeSinceOpen = millis() - _circuitBreakerOpenTime;
        
        if (timeSinceOpen >= CIRCUIT_BREAKER_TIMEOUT_MS) {
            // Try one test connection after timeout
            if (timeSinceOpen % CIRCUIT_BREAKER_TEST_INTERVAL_MS < CIRCUIT_BREAKER_TEST_MOD_MS) {
                LOG_INFOF("Circuit breaker: Testing connection (failure count: %d)\n", _consecutiveFailures);
                _circuitBreakerOpen = false;
                _consecutiveFailures = 0;  // Reset on test attempt
            } else {
                return;  // Still waiting
            }
        } else {
            return;  // Circuit breaker still open
        }
    }
    
    // Si hay fallo de autenticación, aplicar backoff exponencial con jitter
    if (_authFailed) {
        unsigned long baseDelay = min(AUTH_BACKOFF_BASE_MS * (1 << (_authFailureCount - 1)), AUTH_BACKOFF_MAX_MS);
        // Add ±10% random jitter to prevent thundering herd
        int jitter = (random(-AUTH_BACKOFF_JITTER_PERCENT, AUTH_BACKOFF_JITTER_PERCENT + 1) * baseDelay) / 100;
        unsigned long backoffDelay = baseDelay + jitter;
        
        if (millis() - _lastAuthAttempt < backoffDelay) {
            // Aún esperando el backoff
            return;
        }
        // Reset y permitir reconexión
        _authFailed = false;
        DEBUG_PRINTLN("Retrying authentication...");
    }
    
    _webSocket.loop();
    
    // Intelligent heartbeat: only send ping if no activity in last 30 seconds
    if (_connected && (millis() - _lastPing > WS_PING_IDLE_THRESHOLD_MS)) {
        unsigned long timeSinceActivity = millis() - _lastActivity;
        
        // Only send ping if we haven't sent/received any message recently
        if (timeSinceActivity >= WS_PING_IDLE_THRESHOLD_MS) {
            _lastPing = millis();
            StaticJsonDocument<64> doc;
            doc["type"] = "ping";
            doc["device_id"] = DEVICE_ID;
            sendEvent("ping", doc);
            DEBUG_PRINTLN("♡ Heartbeat (no recent activity)");
        } else {
            // Activity detected, reset ping timer
            _lastPing = millis();
        }
    }
}

bool VPSWebSocketClient::isConnected() {
    return _connected;
}

void VPSWebSocketClient::webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    if (_instance) {
        switch(type) {
            case WStype_DISCONNECTED:
                _instance->handleDisconnected();
                break;
                
            case WStype_CONNECTED:
                _instance->handleConnected();
                break;
                
            case WStype_TEXT:
                _instance->handleMessage(payload, length);
                break;
                
            case WStype_ERROR:
                DEBUG_PRINTF("WebSocket Error: %s\n", payload);
                break;
                
            default:
                break;
        }
    }
}

void VPSWebSocketClient::handleConnected() {
    _connected = true;
    _metrics.totalConnections++;
    _metrics.lastConnectionTime = millis() / 1000;
    _lastActivity = millis();  // Reset activity timer on new connection
    
    // Reset circuit breaker on successful connection
    _consecutiveFailures = 0;
    _circuitBreakerOpen = false;
    
    if (_metrics.totalConnections > 1) {
        _metrics.reconnections++;
    }
    DEBUG_PRINTLN("[OK] WebSocket connected to VPS!");
}

void VPSWebSocketClient::handleDisconnected() {
    _connected = false;
    _metrics.totalDisconnections++;
    
    // Increment consecutive failures for circuit breaker
    _consecutiveFailures++;
    
    if (_consecutiveFailures >= CIRCUIT_BREAKER_THRESHOLD) {
        _circuitBreakerOpen = true;
        _circuitBreakerOpenTime = millis();
        LOG_ERRORF("Circuit breaker OPEN: %d consecutive failures. Pausing for %lu seconds\n", 
                   _consecutiveFailures, CIRCUIT_BREAKER_TIMEOUT_MS / 1000);
    }
    
    DEBUG_PRINTLN("✗ WebSocket disconnected from VPS");
}

void VPSWebSocketClient::handleMessage(uint8_t * payload, size_t length) {
    // Increment received messages counter and update last activity
    _metrics.messagesReceived++;
    _lastActivity = millis();
    
    if (length == 0) return;
    
    char packetType = payload[0];
    
    if (packetType == '0') {
        DEBUG_PRINTLN("[OK] Connected to server");
        _webSocket.sendTXT("40");
        
        delay(WS_REGISTRATION_DELAY_MS);
        
        unsigned long regStart = millis();
        StaticJsonDocument<256> deviceInfo;
        deviceInfo["device_id"] = DEVICE_ID;
        deviceInfo["device_type"] = "esp32";
        deviceInfo["firmware_version"] = FIRMWARE_VERSION;
        deviceInfo["auth_token"] = DEVICE_AUTH_TOKEN;
        sendEvent("device:register", deviceInfo);
        
        while (millis() - regStart < WS_REGISTRATION_TIMEOUT_MS) {
            delay(LOOP_ITERATION_DELAY_MS);
        }
        
        DEBUG_PRINTLN("[OK] Device registered");
        
        return;
    }
    
    if (packetType == '2') {
        _webSocket.sendTXT("3");
        return;
    }
    
    if (length >= 2 && payload[0] == '4' && payload[1] == '2') {
        const char* jsonStart = (const char*)(payload + 2);
        
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, jsonStart);
        
        if (error) {
            DEBUG_PRINTF("JSON parse error: %s\n", error.c_str());
            return;
        }
        
        if (!doc.is<JsonArray>() || doc.size() < 1) {
            DEBUG_PRINTLN("Invalid Socket.IO event format");
            return;
        }
        
        const char* eventName = doc[0];
        if (!eventName) return;
        
        DEBUG_PRINTF("Event received: %s\n", eventName);
        
        if (strcmp(eventName, "device:auth_success") == 0) {
            DEBUG_PRINTLN("[OK] Authentication successful");
            _authFailed = false;
            _authFailureCount = 0;
            
            // Reset circuit breaker on successful auth
            _consecutiveFailures = 0;
            _circuitBreakerOpen = false;
            
            return;
        } else if (strcmp(eventName, "device:auth_failed") == 0) {
            DEBUG_PRINTLN("✗ Authentication FAILED - invalid token!");
            _connected = false;
            _authFailed = true;
            _authFailureCount++;
            _lastAuthAttempt = millis();
            _metrics.authFailures++;  // Track auth failures in metrics
            
            // Calcular backoff exponencial con jitter: 30s, 60s, 120s, 240s, max 5 minutos
            unsigned long baseDelay = min(AUTH_BACKOFF_BASE_MS * (1 << (_authFailureCount - 1)), AUTH_BACKOFF_MAX_MS);
            // Add ±10% random jitter to prevent thundering herd
            int jitter = (random(-AUTH_BACKOFF_JITTER_PERCENT, AUTH_BACKOFF_JITTER_PERCENT + 1) * baseDelay) / 100;
            unsigned long backoffDelay = baseDelay + jitter;
            DEBUG_PRINTF("⚠ Retry after %.1f seconds (attempt %d)\n", backoffDelay / 1000.0, _authFailureCount);
            
            if (_authFailureCount >= 5) {
                DEBUG_PRINTLN("⚠ Too many auth failures - check your token configuration!");
            }
            
            return;
        } else if (strcmp(eventName, "relay:command") == 0 && doc.size() >= 2) {
            JsonObject data = doc[1];
            if (!data.isNull()) {
                handleRelayCommand(data);
            }
        } else if (strcmp(eventName, "sensor:request") == 0) {
            handleSensorRequest();
        } else if (strcmp(eventName, "ping") == 0) {
            StaticJsonDocument<64> response;
            response["type"] = "pong";
            sendEvent("pong", response);
        }
    }
}

void VPSWebSocketClient::handleRelayCommand(JsonObject& data) {
    if (!data.containsKey("relay_id") || !data.containsKey("state")) {
        DEBUG_PRINTLN("⚠ Missing relay_id or state in command");
        return;
    }
    
    int relayId = data["relay_id"];
    bool state = data["state"];
    
    if (relayId < 0 || relayId >= 4) {
        DEBUG_PRINTF("⚠ Invalid relay_id: %d (valid: 0-3)\n", relayId);
        
        StaticJsonDocument<128> error;
        error["error"] = "invalid_relay_id";
        error["relay_id"] = relayId;
        sendEvent("relay:error", error);
        return;
    }
    
    if (_relayCommandCallback) {
        _relayCommandCallback(relayId, state);
    }
}

void VPSWebSocketClient::handleSensorRequest() {
    DEBUG_PRINTLN("Sensor data request received");
    
    if (_sensorRequestCallback) {
        _sensorRequestCallback();
    }
}

bool VPSWebSocketClient::sendSensorData(float temperature, float humidity, float soilMoisture, int tempErrors, int humidityErrors) {
    if (!_connected) {
        DEBUG_PRINTLN("Cannot send sensor data: not connected");
        return false;
    }
    
    StaticJsonDocument<256> data;
    data["device_id"] = DEVICE_ID;
    data["temperature"] = temperature;
    data["humidity"] = humidity;
    data["temp_errors"] = tempErrors;
    data["humidity_errors"] = humidityErrors;
    
    if (soilMoisture >= 0) {
        data["soil_moisture"] = soilMoisture;
    }
    
    data["timestamp"] = millis();
    
    // Use static buffer to avoid String object allocation
    char payload[512];  // Increased buffer size for safety
    size_t len = 0;
    
    // Safe string building with bounds checking
    len += snprintf(payload + len, sizeof(payload) - len, "42[\"sensor:data\",");
    
    // Measure JSON size before serialization
    size_t json_size = measureJson(data);
    size_t remaining = sizeof(payload) - len - 2;  // -2 for "]" and null terminator
    
    if (json_size > remaining) {
        DEBUG_PRINTLN("ERROR: JSON payload too large for buffer!");
        return false;
    }
    
    len += serializeJson(data, payload + len, remaining);
    
    // Safe append closing bracket
    if (len < sizeof(payload) - 1) {
        payload[len++] = ']';
        payload[len] = '\0';
    }
    
    _webSocket.sendTXT(payload);
    
    return true;
}

bool VPSWebSocketClient::sendRelayState(int relayId, bool state, const char* mode, const char* changedBy) {
    if (!_connected) {
        DEBUG_PRINTLN("Cannot send relay state: not connected");
        return false;
    }
    
    StaticJsonDocument<256> data;
    data["device_id"] = DEVICE_ID;
    data["relay_id"] = relayId;
    data["state"] = state;
    data["mode"] = mode;
    data["changed_by"] = changedBy;
    data["timestamp"] = millis();
    
    // Use static buffer to avoid String object allocation
    char payload[512];  // Increased buffer size for safety
    size_t len = 0;
    
    // Safe string building with bounds checking
    len += snprintf(payload + len, sizeof(payload) - len, "42[\"relay:state\",");
    
    // Measure JSON size before serialization
    size_t json_size = measureJson(data);
    size_t remaining = sizeof(payload) - len - 2;  // -2 for "]" and null terminator
    
    if (json_size > remaining) {
        DEBUG_PRINTLN("ERROR: Relay state JSON too large for buffer!");
        return false;
    }
    
    len += serializeJson(data, payload + len, remaining);
    
    // Safe append closing bracket
    if (len < sizeof(payload) - 1) {
        payload[len++] = ']';
        payload[len] = '\0';
    }
    
    _webSocket.sendTXT(payload);
    DEBUG_PRINTF("[OK] Relay %d: %s\n", relayId, state ? "ON" : "OFF");
    
    return true;
}

bool VPSWebSocketClient::sendLog(const char* level, const char* message) {
    if (!_connected) {
        return false;
    }
    
    StaticJsonDocument<256> data;
    data["device_id"] = DEVICE_ID;
    data["level"] = level;
    data["message"] = message;
    data["timestamp"] = millis();
    
    // Use static buffer to avoid String object allocation
    char payload[512];  // Increased buffer size for safety
    size_t len = 0;
    
    // Safe string building with bounds checking
    len += snprintf(payload + len, sizeof(payload) - len, "42[\"log\",");
    
    // Measure JSON size before serialization
    size_t json_size = measureJson(data);
    size_t remaining = sizeof(payload) - len - 2;  // -2 for "]" and null terminator
    
    if (json_size > remaining) {
        // Can't log error here (would cause recursion), just fail silently
        return false;
    }
    
    len += serializeJson(data, payload + len, remaining);
    
    // Safe append closing bracket
    if (len < sizeof(payload) - 1) {
        payload[len++] = ']';
        payload[len] = '\0';
    }
    
    _webSocket.sendTXT(payload);
    
    return true;
}

bool VPSWebSocketClient::sendMetrics(const ConnectionMetrics& metrics) {
    if (!_connected) {
        return false;
    }
    
    StaticJsonDocument<384> data;
    data["totalConnections"] = metrics.totalConnections;
    data["authFailures"] = metrics.authFailures;
    data["reconnections"] = metrics.reconnections;
    data["messagesReceived"] = metrics.messagesReceived;
    data["messagesSent"] = metrics.messagesSent;
    data["uptimeSeconds"] = metrics.uptimeSeconds;
    data["lastConnectionTime"] = metrics.lastConnectionTime;
    data["totalDisconnections"] = metrics.totalDisconnections;
    
    // Use static buffer to avoid String object allocation
    char payload[768];  // Larger buffer for metrics (more data)
    size_t len = 0;
    
    // Safe string building with bounds checking
    len += snprintf(payload + len, sizeof(payload) - len, "42[\"metrics\",");
    
    // Measure JSON size before serialization
    size_t json_size = measureJson(data);
    size_t remaining = sizeof(payload) - len - 2;  // -2 for "]" and null terminator
    
    if (json_size > remaining) {
        DEBUG_PRINTLN("ERROR: Metrics JSON too large for buffer!");
        return false;
    }
    
    len += serializeJson(data, payload + len, remaining);
    
    // Safe append closing bracket
    if (len < sizeof(payload) - 1) {
        payload[len++] = ']';
        payload[len] = '\0';
    }
    
    _webSocket.sendTXT(payload);
    
    return true;
}

void VPSWebSocketClient::sendEvent(const char* event, JsonDocument& data) {
    if (!_connected) return;
    
    // Increment sent messages counter and update last activity
    _metrics.messagesSent++;
    _lastActivity = millis();
    
    // Use static buffer to avoid String object allocation
    char payload[768];  // Larger buffer for generic events
    size_t len = 0;
    
    // Safe string building with bounds checking
    len += snprintf(payload + len, sizeof(payload) - len, "42[\"%s\",", event);
    
    // Measure JSON size before serialization
    size_t json_size = measureJson(data);
    size_t remaining = sizeof(payload) - len - 2;  // -2 for "]" and null terminator
    
    if (json_size > remaining) {
        DEBUG_PRINTLN("WARNING: Event JSON too large, truncating");
        // Truncate but still send
    }
    
    len += serializeJson(data, payload + len, remaining);
    
    // Safe append closing bracket
    if (len < sizeof(payload) - 1) {
        payload[len++] = ']';
        payload[len] = '\0';
    }
    
    _webSocket.sendTXT(payload);
}

void VPSWebSocketClient::onRelayCommand(RelayCommandCallback callback) {
    _relayCommandCallback = callback;
}

void VPSWebSocketClient::onSensorRequest(SensorRequestCallback callback) {
    _sensorRequestCallback = callback;
}

ConnectionMetrics VPSWebSocketClient::getMetrics() {
    // Update uptime
    _metrics.uptimeSeconds = (millis() - _startTime) / 1000;
    return _metrics;
}

String VPSWebSocketClient::getStatus() {
    return _connected ? "Connected" : "Disconnected";
}
