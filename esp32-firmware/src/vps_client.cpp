#include "vps_client.h"
#include "config.h"

// VPSClient - Legacy HTTP client (DEPRECATED)
// 
// This file is kept for compatibility but is NOT USED in the current implementation.
// The ESP32 firmware has been fully modernized to use WebSocket communication exclusively.
// 
// All operations that previously used HTTP REST are now handled via Socket.IO WebSocket:
// - Sensor data: WebSocket 'sensor:data' event
// - Relay commands: WebSocket 'relay:state' event
// - Logs: WebSocket 'log' event
// - Health checks: WebSocket 'metrics' event + /health REST endpoint
// 
// See vps_websocket.cpp for the current WebSocket implementation.

VPSClient::VPSClient() {
    _lastError = "";
    _isConnected = false;
}

void VPSClient::begin() {
    DEBUG_PRINTLN("[DEPRECATED] VPSClient (HTTP) is no longer used. WebSocket is primary transport.");
    _isConnected = false;  // Always false - use WebSocket instead
}

int VPSClient::makeRequest(const String& endpoint, const String& method, const String& payload, String& response) {
    DEBUG_PRINTLN("[DEPRECATED] HTTP request attempted - WebSocket should be used instead");
    _lastError = "HTTP client is deprecated - use WebSocket";
    return -1;
}

void VPSClient::setLastError(const String& error) {
    _lastError = error;
}

String VPSClient::getLastError() const {
    return _lastError;
}
