#include "vps_websocket.h"
#include "config.h"

VPSWebSocketClient* VPSWebSocketClient::_instance = nullptr;

VPSWebSocketClient::VPSWebSocketClient() {
    _connected = false;
    _lastReconnectAttempt = 0;
    _lastPing = 0;
    _authFailed = false;
    _authFailureCount = 0;
    _lastAuthAttempt = 0;
    _relayCommandCallback = nullptr;
    _sensorRequestCallback = nullptr;
    _instance = this;
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
    _webSocket.enableHeartbeat(15000, 3000, 2);
    _webSocket.setReconnectInterval(5000);
    
    return true;
}

void VPSWebSocketClient::loop() {
    // Si hay fallo de autenticación, aplicar backoff exponencial
    if (_authFailed) {
        unsigned long backoffDelay = min(30000UL * (1 << (_authFailureCount - 1)), 300000UL);
        if (millis() - _lastAuthAttempt < backoffDelay) {
            // Aún esperando el backoff
            return;
        }
        // Reset y permitir reconexión
        _authFailed = false;
        DEBUG_PRINTLN("Retrying authentication...");
    }
    
    _webSocket.loop();
    
    if (_connected && (millis() - _lastPing > 30000)) {
        _lastPing = millis();
        StaticJsonDocument<64> doc;
        doc["type"] = "ping";
        doc["device_id"] = DEVICE_ID;
        sendEvent("ping", doc);
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
    DEBUG_PRINTLN("✓ WebSocket connected to VPS!");
}

void VPSWebSocketClient::handleDisconnected() {
    _connected = false;
    DEBUG_PRINTLN("✗ WebSocket disconnected from VPS");
}

void VPSWebSocketClient::handleMessage(uint8_t * payload, size_t length) {
    if (length < 1) return;
    
    char packetType = payload[0];
    
    if (packetType == '0') {
        DEBUG_PRINTLN("✓ Connected to server");
        _webSocket.sendTXT("40");
        
        delay(100);
        
        unsigned long regStart = millis();
        StaticJsonDocument<256> deviceInfo;
        deviceInfo["device_id"] = DEVICE_ID;
        deviceInfo["device_type"] = "esp32";
        deviceInfo["firmware_version"] = FIRMWARE_VERSION;
        deviceInfo["auth_token"] = DEVICE_AUTH_TOKEN;
        sendEvent("device:register", deviceInfo);
        
        while (millis() - regStart < 3000) {
            delay(10);
        }
        
        DEBUG_PRINTLN("✓ Device registered");
        
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
            DEBUG_PRINTLN("✓ Authentication successful");
            _authFailed = false;
            _authFailureCount = 0;
            return;
        } else if (strcmp(eventName, "device:auth_failed") == 0) {
            DEBUG_PRINTLN("✗ Authentication FAILED - invalid token!");
            _connected = false;
            _authFailed = true;
            _authFailureCount++;
            _lastAuthAttempt = millis();
            
            // Calcular backoff exponencial: 30s, 60s, 120s, 240s, max 5 minutos
            unsigned long backoffDelay = min(30000UL * (1 << (_authFailureCount - 1)), 300000UL);
            DEBUG_PRINTF("⚠ Retry after %lu seconds (attempt %d)\n", backoffDelay / 1000, _authFailureCount);
            
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

bool VPSWebSocketClient::sendSensorData(float temperature, float humidity, float soilMoisture) {
    if (!_connected) {
        DEBUG_PRINTLN("Cannot send sensor data: not connected");
        return false;
    }
    
    StaticJsonDocument<256> data;
    data["device_id"] = DEVICE_ID;
    data["temperature"] = temperature;
    data["humidity"] = humidity;
    
    if (soilMoisture >= 0) {
        data["soil_moisture"] = soilMoisture;
    }
    
    data["timestamp"] = millis();
    
    String payload = "42[\"sensor:data\",";
    serializeJson(data, payload);
    payload += "]";
    
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
    
    String payload = "42[\"relay:state\",";
    serializeJson(data, payload);
    payload += "]";
    
    _webSocket.sendTXT(payload);
    DEBUG_PRINTF("✓ Relay %d: %s\n", relayId, state ? "ON" : "OFF");
    
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
    
    String payload = "42[\"log\",";
    serializeJson(data, payload);
    payload += "]";
    
    _webSocket.sendTXT(payload);
    
    return true;
}

void VPSWebSocketClient::sendEvent(const char* event, JsonDocument& data) {
    if (!_connected) return;
    
    String payload = "42[\"";
    payload += event;
    payload += "\",";
    serializeJson(data, payload);
    payload += "]";
    
    _webSocket.sendTXT(payload);
}

void VPSWebSocketClient::onRelayCommand(RelayCommandCallback callback) {
    _relayCommandCallback = callback;
}

void VPSWebSocketClient::onSensorRequest(SensorRequestCallback callback) {
    _sensorRequestCallback = callback;
}

String VPSWebSocketClient::getStatus() {
    return _connected ? "Connected" : "Disconnected";
}
