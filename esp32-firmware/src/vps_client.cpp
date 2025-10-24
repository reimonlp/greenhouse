#include "vps_client.h"
#include "vps_config.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// For SSL certificate verification
#ifdef VERIFY_SSL_CERT
extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
#endif

WiFiClientSecure secureClient;

VPSClient::VPSClient() {
    _lastError = "";
    _isConnected = false;
    
    // SSL Certificate Verification Configuration
    // For production: Set VERIFY_SSL_CERT to enable certificate validation
    #ifdef VERIFY_SSL_CERT
        // Load root CA certificate for SSL verification
        // This prevents MITM attacks by validating server identity
        secureClient.setCACertBundle(x509_crt_bundle_start);  // Use built-in CA bundle
        DEBUG_PRINTLN("[OK] SSL certificate verification ENABLED");
    #else
        // Development mode: Skip certificate verification
        // WARNING: Vulnerable to MITM attacks - DO NOT use in production!
        secureClient.setInsecure();
        DEBUG_PRINTLN("[WARN] SSL certificate verification DISABLED (development mode)");
        DEBUG_PRINTLN("       Enable with -DVERIFY_SSL_CERT in platformio.ini for production");
    #endif
}

void VPSClient::begin() {
    DEBUG_PRINTLN("VPSClient initialized for reimon.dev (HTTPS)");
    _isConnected = true;
}

int VPSClient::makeRequest(const String& endpoint, const String& method, const String& payload, String& response) {
    if (!_isConnected) {
        DEBUG_PRINTLN("VPSClient not connected");
    _lastError = error;
}

// Envío de datos por WebSocket únicamente. Función HTTP deshabilitada.

// ========================================
// LEGACY HTTP API - Currently unused (WebSocket preferred)
// ========================================

/*
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
        DEBUG_PRINTLN("[OK] Relay state sent successfully");
    } else {
        DEBUG_PRINTF("[ERROR] Failed to send relay state: %s\n", _lastError.c_str());
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
    
    DEBUG_PRINTLN("[OK] Relay states retrieved successfully");
    return true;
}

String VPSClient::getRules(int relayId) {
    String endpoint = VPS_ENDPOINT_RULES;
    if (relayId >= 0) {
        endpoint += "?relay_id=" + String(relayId);
    }
    
    String response;
    if (!makeRequest("GET", endpoint.c_str(), nullptr, &response)) {
        DEBUG_PRINTF("[ERROR] Failed to get rules: %s\n", _lastError.c_str());
        return "";
    }
    
    DEBUG_PRINTLN("[OK] Rules retrieved successfully");
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
        DEBUG_PRINTLN("[OK] Rule created successfully");
    } else {
        DEBUG_PRINTF("[ERROR] Failed to create rule: %s\n", _lastError.c_str());
    }
    
    return result;
}

bool VPSClient::deleteRule(const String& ruleId) {
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), VPS_ENDPOINT_RULE_BY_ID, ruleId.c_str());
    
    DEBUG_PRINTF("Deleting rule %s\n", ruleId.c_str());
    bool result = makeRequest("DELETE", endpoint, nullptr, nullptr);
    
    if (result) {
        DEBUG_PRINTLN("[OK] Rule deleted successfully");
    } else {
        DEBUG_PRINTF("[ERROR] Failed to delete rule: %s\n", _lastError.c_str());
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
        DEBUG_PRINTLN("[ERROR] VPS health check: Invalid response (NULL fields)");
        DEBUG_PRINTF("  Response: %s\n", response.c_str());
        return false;
    }
    
    bool healthy = (strcmp(status, "ok") == 0 && strcmp(database, "connected") == 0);
    
    if (healthy) {
        DEBUG_PRINTLN("[OK] VPS health check: OK");
    } else {
        DEBUG_PRINTF("[WARN] VPS health check: status=%s, database=%s\n", status, database);
    }
    
    return healthy;
}

bool VPSClient::syncRules() {
    DEBUG_PRINTLN("Syncing rules from VPS...");
    String rulesJson = getRules();
    return !rulesJson.isEmpty();
}
*/
