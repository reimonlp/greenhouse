#include "vps_client.h"
#include "config.h"

VPSClient::VPSClient() {
    _lastError = "";
}

bool VPSClient::makeRequest(const char* method, const char* endpoint, const char* payload, String* response) {
    String url = buildUrl(endpoint);
    
    _http.begin(url);
    _http.setTimeout(HTTP_TIMEOUT_MS);
    _http.addHeader("Content-Type", "application/json");
    
    int httpCode = -1;
    int retries = 0;
    
    while (retries < HTTP_MAX_RETRIES && httpCode < 0) {
        if (strcmp(method, "GET") == 0) {
            httpCode = _http.GET();
        } else if (strcmp(method, "POST") == 0) {
            httpCode = _http.POST(payload ? payload : "");
        } else if (strcmp(method, "PUT") == 0) {
            httpCode = _http.PUT(payload ? payload : "");
        } else if (strcmp(method, "DELETE") == 0) {
            httpCode = _http.sendRequest("DELETE");
        }
        
        if (httpCode < 0) {
            retries++;
            if (retries < HTTP_MAX_RETRIES) {
                DEBUG_PRINTF("HTTP request failed, retry %d/%d\n", retries, HTTP_MAX_RETRIES);
                delay(HTTP_RETRY_DELAY_MS);
            }
        }
    }
    
    bool success = false;
    
    if (httpCode > 0) {
        if (httpCode == 200 || httpCode == 201) {
            if (response) {
                *response = _http.getString();
            }
            success = true;
        } else {
            setLastError("HTTP " + String(httpCode) + ": " + _http.getString());
            DEBUG_PRINTF("HTTP error %d: %s\n", httpCode, _http.getString().c_str());
        }
    } else {
        setLastError("Connection failed: " + _http.errorToString(httpCode));
        DEBUG_PRINTF("HTTP connection failed: %s\n", _http.errorToString(httpCode).c_str());
    }
    
    _http.end();
    return success;
}

String VPSClient::buildUrl(const char* endpoint) {
    return String(VPS_API_BASE_URL) + String(endpoint);
}

void VPSClient::setLastError(const String& error) {
    _lastError = error;
}

bool VPSClient::sendSensorData(float temperature, float humidity, float soilMoisture) {
    StaticJsonDocument<512> doc;  // Increased from 256 to 512
    doc["device_id"] = DEVICE_ID;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    
    if (soilMoisture >= 0) {
        doc["soil_moisture"] = soilMoisture;
    }
    
    String payload;
    serializeJson(doc, payload);
    
    DEBUG_PRINTLN("Sending sensor data to VPS...");
    bool result = makeRequest("POST", VPS_ENDPOINT_SENSORS, payload.c_str(), nullptr);
    
    if (result) {
        DEBUG_PRINTLN("✓ Sensor data sent successfully");
    } else {
        DEBUG_PRINTF("✗ Failed to send sensor data: %s\n", _lastError.c_str());
    }
    
    return result;
}

bool VPSClient::sendRelayState(int relayId, bool state, const char* mode, const char* changedBy) {
    StaticJsonDocument<256> doc;  // Increased from 128 to 256
    doc["state"] = state;
    doc["mode"] = mode;
    doc["changed_by"] = changedBy;
    
    String payload;
    serializeJson(doc, payload);
    
    char endpoint[64];
    snprintf(endpoint, sizeof(endpoint), VPS_ENDPOINT_RELAY_STATE, relayId);
    
    DEBUG_PRINTF("Sending relay %d state: %s\n", relayId, state ? "ON" : "OFF");
    bool result = makeRequest("POST", endpoint, payload.c_str(), nullptr);
    
    if (result) {
        DEBUG_PRINTLN("✓ Relay state sent successfully");
    } else {
        DEBUG_PRINTF("✗ Failed to send relay state: %s\n", _lastError.c_str());
    }
    
    return result;
}

bool VPSClient::getRelayStates(bool states[4]) {
    String response;
    
    if (!makeRequest("GET", VPS_ENDPOINT_RELAY_STATES, nullptr, &response)) {
        DEBUG_PRINTF("✗ Failed to get relay states: %s\n", _lastError.c_str());
        return false;
    }
    
    StaticJsonDocument<2048> doc;  // Increased to 2048 for relay states array with all MongoDB fields
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        setLastError("JSON parse error: " + String(error.c_str()));
        DEBUG_PRINTF("✗ JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    if (!doc["success"]) {
        setLastError("API returned success=false");
        return false;
    }
    
    JsonArray data = doc["data"];
    for (JsonObject relay : data) {
        int relay_id = relay["relay_id"];
        if (relay_id >= 0 && relay_id < 4) {
            states[relay_id] = relay["state"];
        }
    }
    
    DEBUG_PRINTLN("✓ Relay states retrieved successfully");
    return true;
}

String VPSClient::getRules(int relayId) {
    String endpoint = VPS_ENDPOINT_RULES;
    if (relayId >= 0) {
        endpoint += "?relay_id=" + String(relayId);
    }
    
    String response;
    if (!makeRequest("GET", endpoint.c_str(), nullptr, &response)) {
        DEBUG_PRINTF("✗ Failed to get rules: %s\n", _lastError.c_str());
        return "";
    }
    
    DEBUG_PRINTLN("✓ Rules retrieved successfully");
    return response;
}

bool VPSClient::createRule(int relayId, const char* sensor, const char* op, float threshold, const char* action) {
    StaticJsonDocument<512> doc;  // Increased from 256 to 512
    doc["relay_id"] = relayId;
    doc["enabled"] = true;
    
    JsonObject condition = doc.createNestedObject("condition");
    condition["sensor"] = sensor;
    condition["operator"] = op;
    condition["threshold"] = threshold;
    
    doc["action"] = action;
    doc["name"] = String("Rule for relay ") + String(relayId);
    
    String payload;
    serializeJson(doc, payload);
    
    DEBUG_PRINTF("Creating rule for relay %d\n", relayId);
    bool result = makeRequest("POST", VPS_ENDPOINT_RULES, payload.c_str(), nullptr);
    
    if (result) {
        DEBUG_PRINTLN("✓ Rule created successfully");
    } else {
        DEBUG_PRINTF("✗ Failed to create rule: %s\n", _lastError.c_str());
    }
    
    return result;
}

bool VPSClient::deleteRule(const String& ruleId) {
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), VPS_ENDPOINT_RULE_BY_ID, ruleId.c_str());
    
    DEBUG_PRINTF("Deleting rule %s\n", ruleId.c_str());
    bool result = makeRequest("DELETE", endpoint, nullptr, nullptr);
    
    if (result) {
        DEBUG_PRINTLN("✓ Rule deleted successfully");
    } else {
        DEBUG_PRINTF("✗ Failed to delete rule: %s\n", _lastError.c_str());
    }
    
    return result;
}

bool VPSClient::sendLog(const char* level, const char* message, const char* metadata) {
    StaticJsonDocument<1024> doc;  // Increased from 512 to 1024
    doc["level"] = level;
    doc["message"] = message;
    
    if (metadata) {
        JsonObject meta = doc.createNestedObject("metadata");
        // Simple key-value metadata support
        // For now, just store as string
        meta["data"] = metadata;
    }
    
    String payload;
    serializeJson(doc, payload);
    
    return makeRequest("POST", VPS_ENDPOINT_LOGS, payload.c_str(), nullptr);
}

bool VPSClient::healthCheck() {
    String response;
    
    if (!makeRequest("GET", VPS_ENDPOINT_HEALTH, nullptr, &response)) {
        return false;
    }
    
    StaticJsonDocument<512> doc;  // Increased from 256 to 512
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        return false;
    }
    
    const char* status = doc["status"];
    const char* database = doc["database"];  // Changed from "mongodb" to "database"
    
    // Safety check for NULL pointers
    if (!status || !database) {
        DEBUG_PRINTLN("✗ VPS health check: Invalid response (NULL fields)");
        DEBUG_PRINTF("  Response: %s\n", response.c_str());
        return false;
    }
    
    bool healthy = (strcmp(status, "ok") == 0 && strcmp(database, "connected") == 0);
    
    if (healthy) {
        DEBUG_PRINTLN("✓ VPS health check: OK");
    } else {
        DEBUG_PRINTF("⚠ VPS health check: status=%s, database=%s\n", status, database);
    }
    
    return healthy;
}

bool VPSClient::syncRules() {
    // Placeholder para sincronización completa de reglas
    // Implementar según necesidad
    DEBUG_PRINTLN("Syncing rules from VPS...");
    String rulesJson = getRules();
    return !rulesJson.isEmpty();
}
