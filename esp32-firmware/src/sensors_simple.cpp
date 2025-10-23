// Simplified SensorManager for VPS client mode
// Basic DHT22 reading without complex state management

#include "sensors.h"
#include <Arduino.h>

// Global instance
SensorManager sensors;

SensorManager::SensorManager() {
    // dht is automatically initialized as nullptr by unique_ptr
    lastReadTime = 0;
    soilMoisture1Offset = 0.0;
    readingIndex = 0;
    bufferFull = false;
    lastDhtValid = false;
    lastSoilComplete = false;
    
    // Initialize data
    currentData = {0.0, 0.0, 0.0, 0.0, 0, false};
    lastValidData = currentData;
    
    // Initialize validation tracking
    lastValidTemp = 20.0f;  // Reasonable default
    lastValidHumidity = 50.0f;  // Reasonable default
    consecutiveTempErrors = 0;
    consecutiveHumidityErrors = 0;
    
    // Initialize last measured values
    lastMeasuredTemp = 20.0f;
    lastMeasuredHumidity = 50.0f;
}

SensorManager::~SensorManager() {
    // unique_ptr automatically deletes DHT object - no manual cleanup needed
}

bool SensorManager::begin() {
    DEBUG_PRINTLN("Initializing sensors...");
    
    // Initialize DHT11 with smart pointer (automatic memory management)
    // Using reset() instead of make_unique (C++11 compatible)
    dht.reset(new DHT(DHT_PIN, DHT11));
    dht->begin();
    
    // Initialize soil moisture pins
    pinMode(SOIL_MOISTURE_1_PIN, INPUT);
    
    DEBUG_PRINTLN("[OK] Sensors initialized");
    delay(DHT_INIT_STABILIZE_DELAY_MS);  // Give DHT time to stabilize
    
    return true;
}

bool SensorManager::validateTemperature(float temp) {
    // Check for NaN
    if (isnan(temp)) {
        LOG_WARNF("Temperature validation failed: NaN value\n");
        consecutiveTempErrors++;
        return false;
    }
    
    // Check DHT11 datasheet range (0°C to 50°C)
    if (temp < DHT11_MIN_TEMP || temp > DHT11_MAX_TEMP) {
        LOG_WARNF("Temperature out of range: %.1f°C (valid: %.0f-%.0f°C)\n", 
                  temp, DHT11_MIN_TEMP, DHT11_MAX_TEMP);
        consecutiveTempErrors++;
        return false;
    }
    
    // Check for abrupt changes (possible sensor glitch)
    if (consecutiveTempErrors < SENSOR_MAX_CONSECUTIVE_ERRORS) {
        float change = abs(temp - lastValidTemp);
        if (change > MAX_TEMP_CHANGE_PER_READ) {
            LOG_WARNF("Temperature change too abrupt: %.1f°C change (max: %.1f°C)\n", 
                      change, MAX_TEMP_CHANGE_PER_READ);
            consecutiveTempErrors++;
            return false;
        }
    }
    
    // Valid reading - reset error counter and update last valid
    consecutiveTempErrors = 0;
    lastValidTemp = temp;
    return true;
}

bool SensorManager::validateHumidity(float humidity) {
    // Check for NaN
    if (isnan(humidity)) {
        LOG_WARNF("Humidity validation failed: NaN value\n");
        consecutiveHumidityErrors++;
        return false;
    }
    
    // Check DHT11 datasheet range (20% to 90%)
    if (humidity < DHT11_MIN_HUMIDITY || humidity > DHT11_MAX_HUMIDITY) {
        LOG_WARNF("Humidity out of range: %.1f%% (valid: %.0f-%.0f%%)\n", 
                  humidity, DHT11_MIN_HUMIDITY, DHT11_MAX_HUMIDITY);
        consecutiveHumidityErrors++;
        return false;
    }
    
    // Check for abrupt changes (possible sensor glitch)
    if (consecutiveHumidityErrors < SENSOR_MAX_CONSECUTIVE_ERRORS) {
        float change = abs(humidity - lastValidHumidity);
        if (change > MAX_HUMIDITY_CHANGE_PER_READ) {
            LOG_WARNF("Humidity change too abrupt: %.1f%% change (max: %.1f%%)\n", 
                      change, MAX_HUMIDITY_CHANGE_PER_READ);
            consecutiveHumidityErrors++;
            return false;
        }
    }
    
    // Valid reading - reset error counter and update last valid
    consecutiveHumidityErrors = 0;
    lastValidHumidity = humidity;
    return true;
}

bool SensorManager::readSensors() {
    unsigned long now = millis();
    
    // Rate limiting: minimum interval between reads
    if (now - lastReadTime < SENSOR_READ_MIN_INTERVAL_MS) {
        return false;
    }
    
    lastReadTime = now;
    
    // Read DHT11
    float temp = dht->readTemperature();
    float hum = dht->readHumidity();
    
    // Store last measured values (always, even if invalid)
    lastMeasuredTemp = temp;
    lastMeasuredHumidity = hum;
    
    // Validate readings using new validation functions
    bool tempValid = validateTemperature(temp);
    bool humValid = validateHumidity(hum);
    
    lastDhtValid = (tempValid && humValid);
    
    if (lastDhtValid) {
        currentData.temperature = temp;
        currentData.humidity = hum;
        currentData.timestamp = now;
        currentData.valid = true;
        lastValidData = currentData;
    } else {
        // Check if sensor might be faulty
        if (consecutiveTempErrors >= SENSOR_MAX_CONSECUTIVE_ERRORS) {
            LOG_ERRORF("Temperature sensor validation failed: %d consecutive errors - sensor may be malfunctioning or disconnected\n", 
                       consecutiveTempErrors);
        }
        if (consecutiveHumidityErrors >= SENSOR_MAX_CONSECUTIVE_ERRORS) {
            LOG_ERRORF("Humidity sensor validation failed: %d consecutive errors - sensor may be malfunctioning or disconnected\n", 
                       consecutiveHumidityErrors);
        }
        
        // Use last measured readings when current readings are invalid
        // This shows the actual last sensor reading, even if it was invalid
        currentData.temperature = lastMeasuredTemp;
        currentData.humidity = lastMeasuredHumidity;
        currentData.timestamp = now;
        currentData.valid = false;
    }
    
    // Read soil moisture (simplified)
    float soil1 = readSoilMoisture(SOIL_MOISTURE_1_PIN);
    currentData.soil_moisture_1 = convertSoilMoistureToPercentage(soil1);
    
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
        delay(SOIL_MOISTURE_READ_DELAY_MS);
    }
    
    return sum / (float)samples;
}

float SensorManager::convertSoilMoistureToPercentage(float rawValue) {
    // Simplified calibration
    // Assuming: 4095 = dry (0%), 1500 = wet (100%)
    const float DRY_VALUE = 4095.0;
    const float WET_VALUE = 1500.0;
    float percentage = 100.0 * (DRY_VALUE - rawValue) / (DRY_VALUE - WET_VALUE);
    percentage += soilMoisture1Offset;
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

void SensorManager::setSoilMoistureOffset(float offset) {
    soilMoisture1Offset = offset;
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
