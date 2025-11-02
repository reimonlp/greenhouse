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
#include "ota.h"
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

/**
 * @brief Setup WiFi connection with security considerations
 * 
 * Establishes WiFi connection to the configured network.
 * - Uses WPA2 security (configured in secrets.h)
 * - Implements timeout with watchdog feeding
 * - Logs connection status but NOT SSID for security
 * - Restarts ESP32 if connection fails (critical for operation)
 */
void setupWiFi() {
    DEBUG_PRINTLN("Connecting to WiFi...");
    // Don't log SSID for security (prevents network name disclosure)
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(WIFI_CONNECT_DELAY_MS);
        DEBUG_PRINT(".");
        attempts++;
        if (attempts % 5 == 0) {
            esp_task_wdt_reset(); // Feed watchdog every 2.5 seconds during WiFi connection
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTLN("\n[OK] WiFi connected");
        DEBUG_PRINT("IP address: ");
        DEBUG_PRINTLN(WiFi.localIP());
        DEBUG_PRINT("Signal strength: ");
        DEBUG_PRINT(WiFi.RSSI());
        DEBUG_PRINTLN(" dBm");
    } else {
        DEBUG_PRINTLN("\n✗ WiFi connection failed!");
        DEBUG_PRINTLN("Restarting in 5 seconds...");
        delay(WIFI_FAILED_RESTART_DELAY_MS);
        ESP.restart();
    }
}

/**
 * @brief Synchronize system time with NTP server
 * 
 * Configures NTP client for accurate timestamps.
 * - Required for sensor data timestamping
 * - Uses GMT offset and daylight saving configuration
 * - Non-blocking with timeout (continues if NTP fails)
 * - Critical for data logging and rule evaluation
 */
void setupNTP() {
    DEBUG_PRINTLN("Syncing time...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    struct tm timeinfo;
    int attempts = 0;
    while (!getLocalTime(&timeinfo) && attempts < 10) {
        DEBUG_PRINT(".");
        delay(NTP_SYNC_RETRY_DELAY_MS);
        attempts++;
    }
    
    if (attempts < 10) {
        DEBUG_PRINTLN("[OK] Time synchronized");
        DEBUG_PRINTLN(&timeinfo, "%Y-%m-%d %H:%M:%S");
    } else {
        DEBUG_PRINTLN("[WARN] Time sync failed");
    }
}

void checkVPSHealth() {
    if (millis() - lastHealthCheck < HEALTH_CHECK_INTERVAL_MS) {
        return;
    }
    lastHealthCheck = millis();
    
    vpsConnected = vpsWebSocket.isConnected();
    
    if (vpsConnected) {
        failedRequests = 0;
        DEBUG_PRINTLN("[OK] WebSocket connected - system healthy");
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
        DEBUG_PRINTLN("\n[OTA] Update Started: " + type);
        // Disable watchdog during OTA update
        esp_task_wdt_delete(NULL);
    });
    
    ArduinoOTA.onEnd([]() {
        DEBUG_PRINTLN("\n[OTA] Update Completed");
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
        Serial.printf("[ERROR] OTA Error[%u]: ", error);
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
    DEBUG_PRINTLN("[OK] OTA Ready");
    DEBUG_PRINT("  Hostname: ");
    DEBUG_PRINTLN(OTA_HOSTNAME);
    DEBUG_PRINT("  Port: ");
    DEBUG_PRINTLN(OTA_PORT);
    DEBUG_PRINTLN("  Password: ********");
#else
    DEBUG_PRINTLN("⚠ OTA disabled in config");
#endif
}

/**
 * @brief Send current sensor readings to VPS via WebSocket
 * 
 * Reads sensor data and transmits to backend server with error handling:
 * - Rate-limited to prevent flooding (SENSOR_SEND_INTERVAL_MS)
 * - Validates sensor readings before transmission
 * - Includes error counters for sensor health monitoring
 * - Tracks consecutive failures for circuit breaker pattern
 * - Non-blocking operation with timeout handling
 * 
 * Critical for real-time greenhouse monitoring and automation.
 */
void sendSensorData() {
    if (millis() - lastSensorSend < SENSOR_READ_INTERVAL_MS) {
        return;
    }
    lastSensorSend = millis();
    
    DEBUG_PRINTLN("\n=== Sending Sensor Data ===");
    
    sensors.readSensors();
    SensorData data = sensors.getCurrentData();
    
    float temp = data.temperature;
    float hum = data.humidity;
    int tempErrors = sensors.getTempErrors();
    int humErrors = sensors.getHumidityErrors();
    
    if (isnan(temp) || isnan(hum)) {
        DEBUG_PRINTLN("✗ Invalid sensor readings, skipping");
        return;
    }
    
    bool success = vpsWebSocket.sendSensorData(temp, hum, data.soil_moisture, tempErrors, humErrors);
    
    if (!success) {
        failedRequests++;
        DEBUG_PRINTF("Failed requests: %d/%d\n", failedRequests, MAX_FAILED_REQUESTS);
    } else {
        failedRequests = 0;
    }
}

void sendMetrics() {
    // Send metrics every 5 minutes
    if (millis() - lastMetricsSend < METRICS_SEND_INTERVAL_MS) {
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
        DEBUG_PRINTLN("[OK] Metrics sent");
    } else {
        DEBUG_PRINTLN("[ERROR] Failed to send metrics");
    }
}

/**
 * @brief ESP32 initialization and startup sequence
 * 
 * Performs complete system initialization in the correct order:
 * 1. Serial communication setup
 * 2. Watchdog timer configuration (critical for reliability)
 * 3. Hardware initialization (sensors, relays)
 * 4. Network setup (WiFi, NTP)
 * 5. VPS communication setup (WebSocket, OTA)
 * 
 * This function must complete successfully for the system to operate.
 * Failure in any step may cause ESP32 restart or degraded operation.
 */
void setup() {
    DEBUG_SERIAL_BEGIN(115200);
    delay(SYSTEM_STARTUP_DELAY_MS);
    
    DEBUG_PRINTLN("\n\n");
    DEBUG_PRINTLN("╔══════════════════════════════════════════════╗");
    DEBUG_PRINTLN("║  ESP32 Greenhouse - VPS Client Mode          ║");
    DEBUG_PRINTLN("║  Firmware v2.3-ota - OTA Enabled             ║");
    DEBUG_PRINTLN("╚══════════════════════════════════════════════╝");
    DEBUG_PRINTLN();
    
    // Configure and enable watchdog timer
    DEBUG_PRINTLN("=== Initializing Watchdog Timer ===");
    esp_task_wdt_init(WDT_TIMEOUT, true); // 30s timeout, panic on timeout
    esp_task_wdt_add(NULL); // Add current task to WDT watch
    DEBUG_PRINTF("[OK] Watchdog enabled (%d seconds)\n", WDT_TIMEOUT);
    
    DEBUG_PRINTLN("\n=== Initializing Hardware ===");
    relays.begin();
    sensors.begin();
    DEBUG_PRINTLN("[OK] Hardware initialized");
    
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
        delay(WS_CONNECTION_CHECK_DELAY_MS);
        DEBUG_PRINT(".");
        esp_task_wdt_reset(); // Feed watchdog during initialization
        attempts++;
    }
    DEBUG_PRINTLN();
    
    if (vpsWebSocket.isConnected()) {
        DEBUG_PRINTLN("[OK] WebSocket connected!");
        
        vpsWebSocket.sendLog("info", "ESP32 Greenhouse started - WebSocket mode");
        
        DEBUG_PRINTLN("\n=== Sending Initial Relay States ===");
        for (int i = 0; i < 4; i++) {
            bool state = relays.getRelayState(i);
            vpsWebSocket.sendRelayState(i, state, "manual", "system");
            DEBUG_PRINTF("Relay %d initial state: %s\n", i, state ? "ON" : "OFF");
            delay(RELAY_STATE_SEND_DELAY_MS);
        }
        DEBUG_PRINTLN("[OK] Initial relay states sent");
        
        delay(WS_INITIAL_STATE_DELAY_MS);
        sendSensorData();
    } else {
        DEBUG_PRINTLN("⚠ WebSocket connection failed, will retry in loop");
    }
    
    DEBUG_PRINTLN("\n=== Setup Complete ===");
    DEBUG_PRINTLN("Entering main loop...\n");
}

/**
 * @brief Main system loop - executes continuously after setup()
 * 
 * Performs all ongoing system operations in priority order:
 * 1. Watchdog feeding (prevents system reset)
 * 2. OTA update handling (allows remote firmware updates)
 * 3. WebSocket communication maintenance
 * 4. VPS connectivity health checks
 * 5. Sensor data transmission
 * 6. System metrics reporting
 * 
 * This loop must execute reliably for continuous greenhouse operation.
 * All operations are designed to be non-blocking to maintain responsiveness.
 */
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
    delay(LOOP_ITERATION_DELAY_MS);
    yield();
}
