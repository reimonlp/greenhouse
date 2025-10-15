// ESP32 VPS Client - Versión simplificada para enviar datos al VPS
// Sin AsyncWebServer, solo WiFi + HTTPClient

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "vps_config.h"
#include "vps_client.h"
#include "sensors.h"
#include "relays.h"

// External instances (defined in their respective .cpp files)
extern SensorManager sensors;
extern RelayManager relays;

// Global instances
VPSClient vpsClient;

// Timers
unsigned long lastSensorSend = 0;
unsigned long lastRelayPoll = 0;
unsigned long lastRulesSync = 0;
unsigned long lastHealthCheck = 0;

// Status tracking
bool vpsConnected = false;
int failedRequests = 0;
const int MAX_FAILED_REQUESTS = 5;

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
    
    // Enviar al VPS
    bool success = vpsClient.sendSensorData(temp, hum);
    
    if (!success) {
        failedRequests++;
        DEBUG_PRINTF("Failed requests: %d/%d\n", failedRequests, MAX_FAILED_REQUESTS);
    } else {
        failedRequests = 0;
    }
}

void pollRelayStates() {
    if (millis() - lastRelayPoll < RELAY_STATE_POLL_MS) {
        return;
    }
    lastRelayPoll = millis();
    
    DEBUG_PRINTLN("\n=== Polling Relay States ===");
    
    bool states[4];
    if (vpsClient.getRelayStates(states)) {
        // Aplicar estados recibidos del VPS
        for (int i = 0; i < 4; i++) {
            relays.setRelay(i, states[i]);
            DEBUG_PRINTF("Relay %d: %s\n", i, states[i] ? "ON" : "OFF");
        }
    } else {
        failedRequests++;
    }
}

void syncRules() {
    if (millis() - lastRulesSync < RULES_SYNC_INTERVAL_MS) {
        return;
    }
    lastRulesSync = millis();
    
    DEBUG_PRINTLN("\n=== Syncing Rules ===");
    
    String rulesJson = vpsClient.getRules();
    if (rulesJson.isEmpty()) {
        DEBUG_PRINTLN("✗ Failed to sync rules");
        failedRequests++;
    } else {
        DEBUG_PRINTLN("✓ Rules synced successfully");
        // TODO: Parsear y aplicar reglas localmente si se necesita
        // Por ahora, las reglas se ejecutan en el servidor
    }
}

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
        
        // Send initial log
        vpsClient.sendLog("info", "ESP32 Greenhouse started - VPS client mode");
        
        // Initial sensor reading
        delay(2000);  // Wait for sensors to stabilize
        sendSensorData();
    } else {
        DEBUG_PRINTLN("⚠ VPS connection failed, will retry");
    }
    
    DEBUG_PRINTLN("\n=== Setup Complete ===");
    DEBUG_PRINTLN("Entering main loop...\n");
}

void loop() {
    // Periodic tasks
    checkVPSHealth();
    sendSensorData();
    pollRelayStates();
    syncRules();
    
    // Small delay to prevent busy-waiting
    delay(100);
    
    // Yield to WiFi stack
    yield();
}
