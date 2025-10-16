#include "vps_websocket.h"
#include "config.h"

// Static instance pointer for callback
VPSWebSocketClient* VPSWebSocketClient::_instance = nullptr;

VPSWebSocketClient::VPSWebSocketClient() {
    _connected = false;
    _lastReconnectAttempt = 0;
    _lastPing = 0;
    _relayCommandCallback = nullptr;
    _sensorRequestCallback = nullptr;
    _instance = this;
}

bool VPSWebSocketClient::begin() {
    DEBUG_PRINTLN("Initializing WebSocket connection...");
    
    // Parse VPS_WEBSOCKET_HOST and VPS_WEBSOCKET_PORT from vps_config.h
    _webSocket.begin(VPS_WEBSOCKET_HOST, VPS_WEBSOCKET_PORT, VPS_WEBSOCKET_PATH);
    
    // Set event handler
    _webSocket.onEvent(webSocketEvent);
    
    // Enable heartbeat
    _webSocket.enableHeartbeat(15000, 3000, 2);
    
    // Set reconnect interval
    _webSocket.setReconnectInterval(5000);
    
    DEBUG_PRINTF("WebSocket configured: %s:%d%s\n", VPS_WEBSOCKET_HOST, VPS_WEBSOCKET_PORT, VPS_WEBSOCKET_PATH);
    
    return true;
}

void VPSWebSocketClient::loop() {
    _webSocket.loop();
    
    // Send periodic ping if connected
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
    
    // Send device registration
    StaticJsonDocument<128> doc;
    doc["type"] = "device_register";
    doc["device_id"] = DEVICE_ID;
    doc["device_type"] = "esp32";
    doc["firmware_version"] = "2.1-websocket";
    
    sendEvent("device:register", doc);
}

void VPSWebSocketClient::handleDisconnected() {
    _connected = false;
    DEBUG_PRINTLN("✗ WebSocket disconnected from VPS");
}

void VPSWebSocketClient::handleMessage(uint8_t * payload, size_t length) {
    DEBUG_PRINTF("WebSocket message: %s\n", payload);
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        DEBUG_PRINTF("JSON parse error: %s\n", error.c_str());
        return;
    }
    
    // Check message type
    const char* eventType = doc["event"];
    if (!eventType) {
        DEBUG_PRINTLN("Message missing 'event' field");
        return;
    }
    
    // Handle different event types
    if (strcmp(eventType, "relay:command") == 0) {
        JsonObject data = doc["data"];
        if (!data.isNull()) {
            handleRelayCommand(data);
        }
    } else if (strcmp(eventType, "sensor:request") == 0) {
        handleSensorRequest();
    } else if (strcmp(eventType, "ping") == 0) {
        // Respond to ping
        StaticJsonDocument<64> response;
        response["type"] = "pong";
        sendEvent("pong", response);
    }
}

void VPSWebSocketClient::handleRelayCommand(JsonObject& data) {
    int relayId = data["relay_id"] | -1;
    bool state = data["state"] | false;
    
    if (relayId >= 0 && relayId < 4) {
        DEBUG_PRINTF("Relay command received: Relay %d -> %s\n", relayId, state ? "ON" : "OFF");
        
        if (_relayCommandCallback) {
            _relayCommandCallback(relayId, state);
        }
    } else {
        DEBUG_PRINTF("Invalid relay_id: %d\n", relayId);
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
    
    StaticJsonDocument<256> doc;
    doc["event"] = "sensor:data";
    
    JsonObject data = doc.createNestedObject("data");
    data["device_id"] = DEVICE_ID;
    data["temperature"] = temperature;
    data["humidity"] = humidity;
    
    if (soilMoisture >= 0) {
        data["soil_moisture"] = soilMoisture;
    }
    
    data["timestamp"] = millis();
    
    String payload;
    serializeJson(doc, payload);
    
    _webSocket.sendTXT(payload);
    DEBUG_PRINTLN("✓ Sensor data sent via WebSocket");
    
    return true;
}

bool VPSWebSocketClient::sendRelayState(int relayId, bool state, const char* mode, const char* changedBy) {
    if (!_connected) {
        DEBUG_PRINTLN("Cannot send relay state: not connected");
        return false;
    }
    
    StaticJsonDocument<256> doc;
    doc["event"] = "relay:state";
    
    JsonObject data = doc.createNestedObject("data");
    data["device_id"] = DEVICE_ID;
    data["relay_id"] = relayId;
    data["state"] = state;
    data["mode"] = mode;
    data["changed_by"] = changedBy;
    data["timestamp"] = millis();
    
    String payload;
    serializeJson(doc, payload);
    
    _webSocket.sendTXT(payload);
    DEBUG_PRINTF("✓ Relay %d state sent via WebSocket: %s\n", relayId, state ? "ON" : "OFF");
    
    return true;
}

bool VPSWebSocketClient::sendLog(const char* level, const char* message) {
    if (!_connected) {
        return false;
    }
    
    StaticJsonDocument<256> doc;
    doc["event"] = "log";
    
    JsonObject data = doc.createNestedObject("data");
    data["device_id"] = DEVICE_ID;
    data["level"] = level;
    data["message"] = message;
    data["timestamp"] = millis();
    
    String payload;
    serializeJson(doc, payload);
    
    _webSocket.sendTXT(payload);
    
    return true;
}

void VPSWebSocketClient::sendEvent(const char* event, JsonDocument& data) {
    if (!_connected) return;
    
    StaticJsonDocument<512> doc;
    doc["event"] = event;
    doc["data"] = data;
    
    String payload;
    serializeJson(doc, payload);
    
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
