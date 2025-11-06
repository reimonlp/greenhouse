// ESP32-C3 Greenhouse - WebSocket Real-Time Communication
// 
// Notes for ESP32-C3 USB JTAG:
// - Uses printf() instead of Serial.println() (prints to USB JTAG)
// - Must use fflush(stdout) after printf to ensure output
// - CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG enabled in platformio.ini

#include <Arduino.h>
#include <stdio.h>
#include <WiFi.h>
#include <time.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "vps_config.h"
#include "vps_websocket.h"
#include "sensors.h"
#include "relays.h"
#include "secrets.h"

// Watchdog configuration
#define WDT_TIMEOUT 30  // 30 seconds watchdog timeout

extern SensorManager sensors;
extern RelayManager relays;

// Global instances
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
    printf("\n=== Sensor Request from WebSocket ===\n");
    fflush(stdout);
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
    printf("Connecting to WiFi...\n");
    fflush(stdout);
    // Don't log SSID for security (prevents network name disclosure)
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(WIFI_CONNECT_DELAY_MS);
        printf(".");
        fflush(stdout);
        attempts++;
        if (attempts % 5 == 0) {
            esp_task_wdt_reset(); // Feed watchdog every 2.5 seconds during WiFi connection
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        printf("\n[OK] WiFi connected\n");
        printf("IP address: %s\n", WiFi.localIP().toString().c_str());
        printf("Signal strength: %d dBm\n", WiFi.RSSI());
        fflush(stdout);
    } else {
        printf("\n✗ WiFi connection failed!\n");
        printf("Restarting in 5 seconds...\n");
        fflush(stdout);
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
    printf("Syncing time...\n");
    fflush(stdout);
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    struct tm timeinfo;
    int attempts = 0;
    while (!getLocalTime(&timeinfo) && attempts < 10) {
        printf(".");
        fflush(stdout);
        delay(NTP_SYNC_RETRY_DELAY_MS);
        attempts++;
    }
    
    if (attempts < 10) {
        printf("[OK] Time synchronized\n");
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        printf("%s\n", buffer);
        fflush(stdout);
    } else {
        printf("[WARN] Time sync failed\n");
        fflush(stdout);
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
        printf("[OK] WebSocket connected - system healthy\n");
        fflush(stdout);
    } else {
        failedRequests++;
        printf("⚠ WebSocket disconnected (%d/%d)\n", failedRequests, MAX_FAILED_REQUESTS);
        fflush(stdout);
        
        if (failedRequests >= MAX_FAILED_REQUESTS) {
            printf("⚠ WebSocket disconnected for too long, restarting...\n");
            fflush(stdout);
            delay(5000);
            ESP.restart();
        }
    }
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
    
    printf("\n=== Sending Sensor Data ===\n");
    fflush(stdout);
    
    sensors.readSensors();
    SensorData data = sensors.getCurrentData();
    
    float temp = data.temperature;
    float hum = data.humidity;
    int tempErrors = sensors.getTempErrors();
    int humErrors = sensors.getHumidityErrors();
    
    if (isnan(temp) || isnan(hum)) {
        printf("✗ Invalid sensor readings, skipping\n");
        fflush(stdout);
        return;
    }
    
    printf("Temperature: %.2f°C, Humidity: %.2f%%\n", temp, hum);
    fflush(stdout);
    
    bool success = vpsWebSocket.sendSensorData(temp, hum, data.soil_moisture, tempErrors, humErrors);
    
    if (!success) {
        failedRequests++;
        printf("Failed requests: %d/%d\n", failedRequests, MAX_FAILED_REQUESTS);
        fflush(stdout);
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
    
    printf("\n=== Sending Connection Metrics ===\n");
    printf("Total Connections: %lu\n", metrics.totalConnections);
    printf("Reconnections: %lu\n", metrics.reconnections);
    printf("Messages Sent: %lu\n", metrics.messagesSent);
    printf("Messages Received: %lu\n", metrics.messagesReceived);
    printf("Uptime: %lu seconds\n", metrics.uptimeSeconds);
    fflush(stdout);
    
    bool success = vpsWebSocket.sendMetrics(metrics);
    if (success) {
        printf("[OK] Metrics sent\n");
        fflush(stdout);
    } else {
        printf("[ERROR] Failed to send metrics\n");
        fflush(stdout);
    }
}

/**
 * @brief ESP32-C3 initialization and startup sequence
 * 
 * Performs complete system initialization in the correct order:
 * 1. Serial communication setup (printf via USB JTAG)
 * 2. Watchdog timer configuration (critical for reliability)
 * 3. Hardware initialization (sensors, relays)
 * 4. Network setup (WiFi, NTP)
 * 5. VPS communication setup (WebSocket)
 * 
 * This function must complete successfully for the system to operate.
 * Failure in any step may cause ESP32 restart or degraded operation.
 */
void setup() {
    delay(SYSTEM_STARTUP_DELAY_MS);
    
    printf("\n\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  ESP32-C3 Greenhouse - WebSocket Client      ║\n");
    printf("║  Firmware v3.0-c3 - Clean Build              ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n");
    fflush(stdout);
    
    // Configure and enable watchdog timer
    printf("=== Initializing Watchdog Timer ===\n");
    esp_task_wdt_init(WDT_TIMEOUT, true); // 30s timeout, panic on timeout
    esp_task_wdt_add(NULL); // Add current task to WDT watch
    printf("[OK] Watchdog enabled (%d seconds)\n", WDT_TIMEOUT);
    fflush(stdout);
    
    printf("\n=== Initializing Hardware ===\n");
    relays.begin();
    sensors.begin();
    printf("[OK] Hardware initialized\n");
    fflush(stdout);
    
    setupWiFi();
    setupNTP();
    
    printf("\n=== Initializing WebSocket ===\n");
    vpsWebSocket.begin();
    
    vpsWebSocket.onRelayCommand(onRelayCommand);
    vpsWebSocket.onSensorRequest(onSensorRequestReceived);
    
    printf("Waiting for WebSocket connection...\n");
    fflush(stdout);
    int attempts = 0;
    while (!vpsWebSocket.isConnected() && attempts < 20) {
        vpsWebSocket.loop();
        delay(WS_CONNECTION_CHECK_DELAY_MS);
        printf(".");
        fflush(stdout);
        esp_task_wdt_reset(); // Feed watchdog during initialization
        attempts++;
    }
    printf("\n");
    fflush(stdout);
    
    if (vpsWebSocket.isConnected()) {
        printf("[OK] WebSocket connected!\n");
        fflush(stdout);
        
        vpsWebSocket.sendLog("info", "ESP32-C3 Greenhouse started");
        
        printf("\n=== Sending Initial Relay States ===\n");
        for (int i = 0; i < 4; i++) {
            bool state = relays.getRelayState(i);
            vpsWebSocket.sendRelayState(i, state, "manual", "system");
            printf("Relay %d initial state: %s\n", i, state ? "ON" : "OFF");
            fflush(stdout);
            delay(RELAY_STATE_SEND_DELAY_MS);
        }
        printf("[OK] Initial relay states sent\n");
        fflush(stdout);
        
        delay(WS_INITIAL_STATE_DELAY_MS);
        sendSensorData();
    } else {
        printf("⚠ WebSocket connection failed, will retry in loop\n");
        fflush(stdout);
    }
    
    printf("\n=== Setup Complete ===\n");
    printf("Entering main loop...\n\n");
    fflush(stdout);
}

/**
 * @brief Main system loop - executes continuously after setup()
 * 
 * Performs all ongoing system operations in priority order:
 * 1. Watchdog feeding (prevents system reset)
 * 2. WebSocket communication maintenance
 * 3. VPS connectivity health checks
 * 4. Sensor data transmission
 * 5. System metrics reporting
 * 
 * This loop must execute reliably for continuous greenhouse operation.
 * All operations are designed to be non-blocking to maintain responsiveness.
 */
void loop() {
    // Feed the watchdog timer at the start of each loop iteration
    esp_task_wdt_reset();
    
    vpsWebSocket.loop();
    checkVPSHealth();
    sendSensorData();
    sendMetrics();
    delay(LOOP_ITERATION_DELAY_MS);
    yield();
}
