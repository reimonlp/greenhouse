#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"
#include <DHT.h>
#include <memory>

/**
 * @class SensorManager
 * @brief Manages all greenhouse sensor operations with validation and error handling
 * 
 * Provides a unified interface for reading temperature, humidity, and soil moisture sensors.
 * Implements robust validation, anomaly detection, and fault tolerance mechanisms.
 * 
 * Key Features:
 * - DHT11 temperature/humidity sensor with range validation
 * - Capacitive soil moisture sensors with calibration
 * - Consecutive error tracking for sensor health monitoring
 * - Automatic fallback to last valid readings on sensor failure
 * - Memory-safe implementation using smart pointers
 */
class SensorManager {
private:
    float externalHumidity;
public:
    void setExternalHumidity(float value);
    void clearExternalHumidity();
    std::unique_ptr<DHT> dht;  // Smart pointer prevents memory leaks
    unsigned long lastReadTime;
    float soilMoisture1Offset;
    int readingIndex;
    bool bufferFull;
    bool lastDhtValid;
    bool lastSoilComplete;
    SensorData currentData;
    SensorData lastValidData;
    
    // Sensor validation tracking
    float lastValidTemp;
    float lastValidHumidity;
    int consecutiveTempErrors;
    int consecutiveHumidityErrors;
    
    // Store last measured values (valid or invalid)
    float lastMeasuredTemp;
    float lastMeasuredHumidity;

    float readSoilMoisture(int pin);
    float convertSoilMoistureToPercentage(float rawValue);
    bool validateTemperature(float temp);
    bool validateHumidity(float humidity);

public:
    SensorManager();
    ~SensorManager();
    bool begin();
    bool readSensors();
    SensorData getCurrentData();
    SensorData getLastValidData();
    void setSoilMoistureOffset(float offset);
    bool isDataValid(const SensorData& data);
    String getLastError();
    int getTempErrors() const { return consecutiveTempErrors; }
    int getHumidityErrors() const { return consecutiveHumidityErrors; }
    bool updateSoilSampling();
    SystemStats getStatistics();
    void resetStatistics();
    void updateStatistics(const SensorData& data);
};

extern SensorManager sensors;

#endif
