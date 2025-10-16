// ESP32 VPS Client - Versión con WebSocket para comunicación en tiempo real
// Sin AsyncWebServer, WiFi + WebSocket

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "vps_config.h"
#include "vps_client.h"
#include "vps_websocket.h"
#include "sensors.h"
#include "relays.h"

// External instances (defined in their respective .cpp files)
extern SensorManager sensors;
extern RelayManager relays;

// Global instances
VPSClient vpsClient;  // Keep for initial HTTP setup
VPSWebSocketClient vpsWebSocket;  // New WebSocket client

// Timers
unsigned long lastSensorSend = 0;
unsigned long lastHealthCheck = 0;

// Status tracking
bool vpsConnected = false;
int failedRequests = 0;
const int MAX_FAILED_REQUESTS = 5;

// Forward declarations
void sendSensorData();

// WebSocket callbacks
void onRelayCommandReceived(int relayId, bool state) {
    DEBUG_PRINTF("\n=== Relay Command from WebSocket ===\n");
    DEBUG_PRINTF("Relay %d -> %s\n", relayId, state ? "ON" : "OFF");
    
    // Apply relay state immediately
    relays.setRelay(relayId, state);
    
    // Send confirmation back via WebSocket
    vpsWebSocket.sendRelayState(relayId, state, "manual", "websocket");
}

void onSensorRequestReceived() {
    DEBUG_PRINTLN("\n=== Sensor Request from WebSocket ===");
    sendSensorData();
}

void setupWiFi() {
    DEBUG_PRINTLN("\n=== Starting WiFi Connection ===");
    DEBUG_PRINT("Connecting to: ");
    DEBUG_PRINTLN(WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        DEBUG_PRINT(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTLN("\n✓ WiFi connected");
        DEBUG_PRINT("IP address: ");
        DEBUG_PRINTLN(WiFi.localIP());
        DEBUG_PRINT("Signal strength: ");
        DEBUG_PRINT(WiFi.RSSI());
        DEBUG_PRINTLN(" dBm");
    } else {
        DEBUG_PRINTLN("\n✗ WiFi connection failed!");
        DEBUG_PRINTLN("Restarting in 5 seconds...");
        delay(5000);
        ESP.restart();
    }
}

void setupNTP() {
    DEBUG_PRINTLN("\n=== Setting up NTP ===");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    struct tm timeinfo;
    int attempts = 0;
    while (!getLocalTime(&timeinfo) && attempts < 10) {
        DEBUG_PRINT(".");
        delay(500);
        attempts++;
    }
    
    if (attempts < 10) {
        DEBUG_PRINTLN("\n✓ NTP time synchronized");
        DEBUG_PRINTLN(&timeinfo, "%Y-%m-%d %H:%M:%S");
    } else {
        DEBUG_PRINTLN("\n⚠ NTP sync failed, continuing anyway");
    }
}

void checkVPSHealth() {
    if (millis() - lastHealthCheck < 60000) {
        return; // Check every minute
    }
    lastHealthCheck = millis();
    
    DEBUG_PRINTLN("\n=== Checking VPS Health ===");
    vpsConnected = vpsClient.healthCheck();
    
    if (vpsConnected) {
        failedRequests = 0;  // Reset counter on successful health check
        DEBUG_PRINTLN("✓ VPS is healthy");
    } else {
        failedRequests++;
        DEBUG_PRINTF("✗ VPS health check failed (%d/%d)\n", failedRequests, MAX_FAILED_REQUESTS);
        
        if (failedRequests >= MAX_FAILED_REQUESTS) {
            DEBUG_PRINTLN("⚠ Too many failed requests, restarting...");
            delay(5000);
            ESP.restart();
        }
    }
}

void sendSensorData() {
    if (millis() - lastSensorSend < SENSOR_SEND_INTERVAL_MS) {
        return;
    }
    lastSensorSend = millis();
    
    DEBUG_PRINTLN("\n=== Sending Sensor Data ===");
    
    // Leer sensores
    sensors.readSensors();
    SensorData data = sensors.getCurrentData();
    
    float temp = data.temperature;
    float hum = data.humidity;
    
    if (isnan(temp) || isnan(hum)) {
        DEBUG_PRINTLN("✗ Invalid sensor readings, skipping");
        return;
    }
    
    DEBUG_PRINTF("Temperature: %.1f°C\n", temp);
    DEBUG_PRINTF("Humidity: %.1f%%\n", hum);
    
    // Enviar al VPS via WebSocket
    bool success = vpsWebSocket.sendSensorData(temp, hum);
    
    if (!success) {
        failedRequests++;
        DEBUG_PRINTF("Failed requests: %d/%d\n", failedRequests, MAX_FAILED_REQUESTS);
    } else {
        failedRequests = 0;
    }
}

// No more polling needed! WebSocket handles relay commands in real-time

void setup() {
    DEBUG_SERIAL_BEGIN(115200);
    delay(1000);
    
    DEBUG_PRINTLN("\n\n");
    DEBUG_PRINTLN("╔══════════════════════════════════════════════╗");
    DEBUG_PRINTLN("║  ESP32 Greenhouse - VPS Client Mode          ║");
    DEBUG_PRINTLN("║  Firmware v2.0 - Cloud Connected             ║");
    DEBUG_PRINTLN("╚══════════════════════════════════════════════╝");
    DEBUG_PRINTLN();
    
    // Initialize hardware
    DEBUG_PRINTLN("=== Initializing Hardware ===");
    relays.begin();
    sensors.begin();
    DEBUG_PRINTLN("✓ Hardware initialized");
    
    // Connect to WiFi
    setupWiFi();
    
    // Setup NTP
    setupNTP();
    
    // Initial health check
    DEBUG_PRINTLN("\n=== Initial VPS Connection ===");
    vpsConnected = vpsClient.healthCheck();
    
    if (vpsConnected) {
        DEBUG_PRINTLN("✓ Connected to VPS successfully");
    } else {
        DEBUG_PRINTLN("⚠ VPS HTTP connection failed");
    }
    
    // Initialize WebSocket connection
    DEBUG_PRINTLN("\n=== Initializing WebSocket ===");
    vpsWebSocket.begin();
    
    // Set up WebSocket callbacks
    vpsWebSocket.onRelayCommand(onRelayCommandReceived);
    vpsWebSocket.onSensorRequest(onSensorRequestReceived);
    
    // Wait for WebSocket connection
    DEBUG_PRINTLN("Waiting for WebSocket connection...");
    int attempts = 0;
    while (!vpsWebSocket.isConnected() && attempts < 20) {
        vpsWebSocket.loop();
        delay(500);
        DEBUG_PRINT(".");
        attempts++;
    }
    DEBUG_PRINTLN();
    
    if (vpsWebSocket.isConnected()) {
        DEBUG_PRINTLN("✓ WebSocket connected!");
        
        // Send initial log
        vpsWebSocket.sendLog("info", "ESP32 Greenhouse started - WebSocket mode");
        
        // Send initial state of all 4 relays via WebSocket
        DEBUG_PRINTLN("\n=== Sending Initial Relay States ===");
        for (int i = 0; i < 4; i++) {
            bool state = relays.getRelayState(i);
            vpsWebSocket.sendRelayState(i, state, "manual", "system");
            DEBUG_PRINTF("Relay %d initial state: %s\n", i, state ? "ON" : "OFF");
            delay(100);  // Small delay between requests
        }
        DEBUG_PRINTLN("✓ Initial relay states sent");
        
        // Initial sensor reading
        delay(2000);  // Wait for sensors to stabilize
        sendSensorData();
    } else {
        DEBUG_PRINTLN("⚠ WebSocket connection failed, will retry in loop");
    }
    
    DEBUG_PRINTLN("\n=== Setup Complete ===");
    DEBUG_PRINTLN("Entering main loop...\n");
}

void loop() {
    // WebSocket loop (must be called frequently)
    vpsWebSocket.loop();
    
    // Periodic tasks
    checkVPSHealth();
    sendSensorData();
    
    // Small delay to prevent busy-waiting
    delay(10);
    
    // Yield to WiFi stack
    yield();
}
