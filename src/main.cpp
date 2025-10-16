// ESP32 Greenhouse - WebSocket Real-Time Communication

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "vps_config.h"
#include "vps_client.h"
#include "vps_websocket.h"
#include "vps_ota.h"
#include "sensors.h"
#include "relays.h"
#include "secrets.h"

// Watchdog configuration
#define WDT_TIMEOUT 30  // 30 seconds watchdog timeout

extern SensorManager sensors;
extern RelayManager relays;

// Global instances
VPSClient vpsClient;
VPSWebSocketClient vpsWebSocket;

// Timers
unsigned long lastSensorSend = 0;
unsigned long lastHealthCheck = 0;
unsigned long lastMetricsSend = 0;

// Status tracking
bool vpsConnected = false;
int failedRequests = 0;
const int MAX_FAILED_REQUESTS = 5;

void sendSensorData();
void sendMetrics();

// WebSocket callbacks
void onRelayCommand(int relayId, bool state) {
    relays.setRelay(relayId, state);
    vpsWebSocket.sendRelayState(relayId, state, "remote", "websocket");
}

void onSensorRequestReceived() {
    DEBUG_PRINTLN("\n=== Sensor Request from WebSocket ===");
    sendSensorData();
}

void setupWiFi() {
    DEBUG_PRINTLN("Connecting WiFi...");
    DEBUG_PRINTLN(WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        DEBUG_PRINT(".");
        attempts++;
        if (attempts % 5 == 0) {
            esp_task_wdt_reset(); // Feed watchdog every 2.5 seconds during WiFi connection
        }
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
    DEBUG_PRINTLN("Syncing time...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    struct tm timeinfo;
    int attempts = 0;
    while (!getLocalTime(&timeinfo) && attempts < 10) {
        DEBUG_PRINT(".");
        delay(500);
        attempts++;
    }
    
    if (attempts < 10) {
        DEBUG_PRINTLN("✓ Time synchronized");
        DEBUG_PRINTLN(&timeinfo, "%Y-%m-%d %H:%M:%S");
    } else {
        DEBUG_PRINTLN("⚠ Time sync failed");
    }
}

void checkVPSHealth() {
    if (millis() - lastHealthCheck < 60000) {
        return;
    }
    lastHealthCheck = millis();
    
    vpsConnected = vpsWebSocket.isConnected();
    
    if (vpsConnected) {
        failedRequests = 0;
        DEBUG_PRINTLN("✓ WebSocket connected - system healthy");
    } else {
        failedRequests++;
        DEBUG_PRINTF("⚠ WebSocket disconnected (%d/%d)\n", failedRequests, MAX_FAILED_REQUESTS);
        
        if (failedRequests >= MAX_FAILED_REQUESTS) {
            DEBUG_PRINTLN("⚠ WebSocket disconnected for too long, restarting...");
            delay(5000);
            ESP.restart();
        }
    }
}

void setupOTA() {
#if OTA_ENABLED
    DEBUG_PRINTLN("Setting up OTA...");
    
    // Set hostname
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    
    // Set OTA port
    ArduinoOTA.setPort(OTA_PORT);
    
    // Set password for security
    ArduinoOTA.setPassword(OTA_PASSWORD);
    
    // OTA callbacks for monitoring
    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        DEBUG_PRINTLN("\n🔄 OTA Update Started: " + type);
        // Disable watchdog during OTA update
        esp_task_wdt_delete(NULL);
    });
    
    ArduinoOTA.onEnd([]() {
        DEBUG_PRINTLN("\n✅ OTA Update Completed");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static unsigned int lastPercent = 0;
        unsigned int percent = (progress / (total / 100));
        if (percent != lastPercent && percent % 10 == 0) {
            DEBUG_PRINTF("OTA Progress: %u%%\n", percent);
            lastPercent = percent;
        }
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("❌ OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Serial.println("End Failed");
        }
        
        // Re-enable watchdog after failed OTA
        esp_task_wdt_add(NULL);
    });
    
    ArduinoOTA.begin();
    DEBUG_PRINTLN("✓ OTA Ready");
    DEBUG_PRINT("  Hostname: ");
    DEBUG_PRINTLN(OTA_HOSTNAME);
    DEBUG_PRINT("  Port: ");
    DEBUG_PRINTLN(OTA_PORT);
    DEBUG_PRINTLN("  Password: ********");
#else
    DEBUG_PRINTLN("⚠ OTA disabled in config");
#endif
}

void sendSensorData() {
    if (millis() - lastSensorSend < SENSOR_SEND_INTERVAL_MS) {
        return;
    }
    lastSensorSend = millis();
    
    DEBUG_PRINTLN("\n=== Sending Sensor Data ===");
    
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
    
    bool success = vpsWebSocket.sendSensorData(temp, hum);
    
    if (!success) {
        failedRequests++;
        DEBUG_PRINTF("Failed requests: %d/%d\n", failedRequests, MAX_FAILED_REQUESTS);
    } else {
        failedRequests = 0;
    }
}

void sendMetrics() {
    // Send metrics every 5 minutes
    if (millis() - lastMetricsSend < 300000) {
        return;
    }
    lastMetricsSend = millis();
    
    if (!vpsWebSocket.isConnected()) {
        return;
    }
    
    ConnectionMetrics metrics = vpsWebSocket.getMetrics();
    
    DEBUG_PRINTLN("\n=== Sending Connection Metrics ===");
    DEBUG_PRINTF("Total Connections: %lu\n", metrics.totalConnections);
    DEBUG_PRINTF("Reconnections: %lu\n", metrics.reconnections);
    DEBUG_PRINTF("Auth Failures: %lu\n", metrics.authFailures);
    DEBUG_PRINTF("Messages Sent: %lu\n", metrics.messagesSent);
    DEBUG_PRINTF("Messages Received: %lu\n", metrics.messagesReceived);
    DEBUG_PRINTF("Uptime: %lu seconds\n", metrics.uptimeSeconds);
    
    bool success = vpsWebSocket.sendMetrics(metrics);
    if (success) {
        DEBUG_PRINTLN("✓ Metrics sent");
    } else {
        DEBUG_PRINTLN("✗ Failed to send metrics");
    }
}

void setup() {
    DEBUG_SERIAL_BEGIN(115200);
    delay(1000);
    
    DEBUG_PRINTLN("\n\n");
    DEBUG_PRINTLN("╔══════════════════════════════════════════════╗");
    DEBUG_PRINTLN("║  ESP32 Greenhouse - VPS Client Mode          ║");
    DEBUG_PRINTLN("║  Firmware v2.2-ota - Cloud Connected         ║");
    DEBUG_PRINTLN("╚══════════════════════════════════════════════╝");
    DEBUG_PRINTLN();
    
    // Configure and enable watchdog timer
    DEBUG_PRINTLN("=== Initializing Watchdog Timer ===");
    esp_task_wdt_init(WDT_TIMEOUT, true); // 30s timeout, panic on timeout
    esp_task_wdt_add(NULL); // Add current task to WDT watch
    DEBUG_PRINTF("✓ Watchdog enabled (%d seconds)\n", WDT_TIMEOUT);
    
    DEBUG_PRINTLN("\n=== Initializing Hardware ===");
    relays.begin();
    sensors.begin();
    DEBUG_PRINTLN("✓ Hardware initialized");
    
    setupWiFi();
    setupNTP();
    setupOTA();
    
    DEBUG_PRINTLN("\n=== Initializing WebSocket ===");
    vpsWebSocket.begin();
    
    vpsWebSocket.onRelayCommand(onRelayCommand);
    vpsWebSocket.onSensorRequest(onSensorRequestReceived);
    
    DEBUG_PRINTLN("Waiting for WebSocket connection...");
    int attempts = 0;
    while (!vpsWebSocket.isConnected() && attempts < 20) {
        vpsWebSocket.loop();
        delay(500);
        DEBUG_PRINT(".");
        esp_task_wdt_reset(); // Feed watchdog during initialization
        attempts++;
    }
    DEBUG_PRINTLN();
    
    if (vpsWebSocket.isConnected()) {
        DEBUG_PRINTLN("✓ WebSocket connected!");
        
        vpsWebSocket.sendLog("info", "ESP32 Greenhouse started - WebSocket mode");
        
        DEBUG_PRINTLN("\n=== Sending Initial Relay States ===");
        for (int i = 0; i < 4; i++) {
            bool state = relays.getRelayState(i);
            vpsWebSocket.sendRelayState(i, state, "manual", "system");
            DEBUG_PRINTF("Relay %d initial state: %s\n", i, state ? "ON" : "OFF");
            delay(100);
        }
        DEBUG_PRINTLN("✓ Initial relay states sent");
        
        delay(2000);
        sendSensorData();
    } else {
        DEBUG_PRINTLN("⚠ WebSocket connection failed, will retry in loop");
    }
    
    DEBUG_PRINTLN("\n=== Setup Complete ===");
    DEBUG_PRINTLN("Entering main loop...\n");
}

void loop() {
    // Feed the watchdog timer at the start of each loop iteration
    esp_task_wdt_reset();
    
    // Handle OTA updates
    #if OTA_ENABLED
    ArduinoOTA.handle();
    #endif
    
    vpsWebSocket.loop();
    checkVPSHealth();
    sendSensorData();
    sendMetrics();
    delay(10);
    yield();
}
