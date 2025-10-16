// Simplified SensorManager for VPS client mode
// Basic DHT22 reading without complex state management

#include "sensors.h"
#include <Arduino.h>

// Global instance
SensorManager sensors;

SensorManager::SensorManager() {
    dht = nullptr;
    lastReadTime = 0;
    soilMoisture1Offset = 0.0;
    soilMoisture2Offset = 0.0;
    readingIndex = 0;
    bufferFull = false;
    lastDhtValid = false;
    lastSoilComplete = false;
    
    // Initialize data
    currentData = {0.0, 0.0, 0.0, 0.0, 0, false};
    lastValidData = currentData;
}

SensorManager::~SensorManager() {
    if (dht) {
        delete dht;
    }
}

bool SensorManager::begin() {
    DEBUG_PRINTLN("Initializing sensors...");
    
    // Initialize DHT11
    dht = new DHT(DHT_PIN, DHT11);
    dht->begin();
    
    // Initialize soil moisture pins
    pinMode(SOIL_MOISTURE_1_PIN, INPUT);
    #ifdef SOIL_MOISTURE_2_PIN
    pinMode(SOIL_MOISTURE_2_PIN, INPUT);
    #endif
    
    DEBUG_PRINTLN("✓ Sensors initialized");
    delay(2000);  // Give DHT time to stabilize
    
    return true;
}

bool SensorManager::readSensors() {
    unsigned long now = millis();
    
    // Rate limiting: read every 2 seconds minimum
    if (now - lastReadTime < 2000) {
        return false;
    }
    
    lastReadTime = now;
    
    // Read DHT22
    float temp = dht->readTemperature();
    float hum = dht->readHumidity();
    
    lastDhtValid = (!isnan(temp) && !isnan(hum) && temp > -40 && temp < 80 && hum >= 0 && hum <= 100);
    
    if (lastDhtValid) {
        currentData.temperature = temp;
        currentData.humidity = hum;
        currentData.timestamp = now;
        currentData.valid = true;
        lastValidData = currentData;
    } else {
        DEBUG_PRINTLN("⚠ DHT22 reading failed");
        currentData.valid = false;
    }
    
    // Read soil moisture (simplified)
    float soil1 = readSoilMoisture(SOIL_MOISTURE_1_PIN);
    currentData.soil_moisture_1 = convertSoilMoistureToPercentage(soil1, 0);
    
    #ifdef SOIL_MOISTURE_2_PIN
    float soil2 = readSoilMoisture(SOIL_MOISTURE_2_PIN);
    currentData.soil_moisture_2 = convertSoilMoistureToPercentage(soil2, 1);
    #else
    currentData.soil_moisture_2 = 0;
    #endif
    
    lastSoilComplete = true;
    
    // Consolidated sensor log with all available data
    if (lastDhtValid) {
        if (currentData.soil_moisture_1 > 0) {
            DEBUG_PRINTF("Sensors: T=%.1f°C H=%.1f%% Soil=%.0f%%\n", temp, hum, currentData.soil_moisture_1);
        } else {
            DEBUG_PRINTF("Sensors: T=%.1f°C H=%.1f%%\n", temp, hum);
        }
    }
    
    return lastDhtValid;
}

float SensorManager::readSoilMoisture(int pin) {
    // Simple average of multiple readings
    uint32_t sum = 0;
    const int samples = 10;
    
    for (int i = 0; i < samples; i++) {
        sum += analogRead(pin);
        delay(10);
    }
    
    return sum / (float)samples;
}

float SensorManager::convertSoilMoistureToPercentage(float rawValue, int sensor) {
    // Simplified calibration
    // Assuming: 4095 = dry (0%), 1500 = wet (100%)
    const float DRY_VALUE = 4095.0;
    const float WET_VALUE = 1500.0;
    
    float percentage = 100.0 * (DRY_VALUE - rawValue) / (DRY_VALUE - WET_VALUE);
    
    // Apply offset if set
    if (sensor == 0) {
        percentage += soilMoisture1Offset;
    } else if (sensor == 1) {
        percentage += soilMoisture2Offset;
    }
    
    // Clamp to 0-100%
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    
    return percentage;
}

SensorData SensorManager::getCurrentData() {
    return currentData;
}

SensorData SensorManager::getLastValidData() {
    return lastValidData;
}

void SensorManager::setSoilMoistureOffset(int sensor, float offset) {
    if (sensor == 0) {
        soilMoisture1Offset = offset;
    } else if (sensor == 1) {
        soilMoisture2Offset = offset;
    }
}

bool SensorManager::isDataValid(const SensorData& data) {
    return data.valid && 
           !isnan(data.temperature) && 
           !isnan(data.humidity) &&
           data.temperature > -40 && data.temperature < 80 &&
           data.humidity >= 0 && data.humidity <= 100;
}

String SensorManager::getLastError() {
    if (!lastDhtValid) {
        return "DHT22 reading failed";
    }
    return "";
}

// Simplified stubs for compatibility
bool SensorManager::updateSoilSampling() { return lastSoilComplete; }
SystemStats SensorManager::getStatistics() { return SystemStats(); }
void SensorManager::resetStatistics() {}
void SensorManager::updateStatistics(const SensorData& data) {}
