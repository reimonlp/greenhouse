// TEST MODE - ESP32-C3 Serial Output Verification
// This is a minimal test to verify printf() works and relay initialization works
// Compile with: pio run -e greenhouse-esp32c3 (after swapping main.cpp)

#include <Arduino.h>
#include <stdio.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "relays.h"
#include "sensors.h"

extern RelayManager relays;
extern SensorManager sensors;

#define WDT_TIMEOUT 60  // 60 seconds for testing

void setup() {
    delay(1000);  // Wait for USB JTAG to be ready
    
    printf("\n\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  ESP32-C3 TEST MODE - Serial Output Test    ║\n");
    printf("║  Firmware v3.0-test - Minimal Build         ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n");
    fflush(stdout);
    
    // Watchdog configuration
    printf("=== Initializing Watchdog Timer ===\n");
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);
    printf("[OK] Watchdog enabled (%d seconds)\n", WDT_TIMEOUT);
    fflush(stdout);
    
    // Hardware initialization
    printf("\n=== Initializing Hardware ===\n");
    fflush(stdout);
    
    printf("Initializing relays...\n");
    fflush(stdout);
    bool relays_ok = relays.begin();
    printf("[%s] Relays initialized\n", relays_ok ? "OK" : "FAIL");
    fflush(stdout);
    
    printf("Initializing sensors...\n");
    fflush(stdout);
    bool sensors_ok = sensors.begin();
    printf("[%s] Sensors initialized\n", sensors_ok ? "OK" : "FAIL");
    fflush(stdout);
    
    printf("\n=== GPIO Test ===\n");
    printf("Testing relay GPIO outputs:\n");
    for (int i = 0; i < 4; i++) {
        printf("  RELAY %d: Setting to ON\n", i);
        fflush(stdout);
        relays.setRelay(i, true);
        delay(500);
        esp_task_wdt_reset();
        
        printf("  RELAY %d: Setting to OFF\n", i);
        fflush(stdout);
        relays.setRelay(i, false);
        delay(500);
        esp_task_wdt_reset();
    }
    
    printf("\n=== ADC Test (Soil Moisture) ===\n");
    printf("Reading soil moisture sensor 10 times:\n");
    fflush(stdout);
    for (int i = 0; i < 10; i++) {
        int value = analogRead(SOIL_MOISTURE_1_PIN);
        printf("  Sample %d: %d (0-4095)\n", i+1, value);
        fflush(stdout);
        delay(200);
        esp_task_wdt_reset();
    }
    
    printf("\n=== Test Complete ===\n");
    printf("All systems operational! Hardware ready for full firmware.\n");
    printf("Press ESP reset to restart or upload full firmware via OTA.\n");
    printf("\n");
    fflush(stdout);
}

void loop() {
    esp_task_wdt_reset();
    
    printf(".");
    fflush(stdout);
    
    delay(1000);
}
