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
    std::unique_ptr<DHT> dht;  // Smart pointer prevents memory leaks
    unsigned long lastReadTime;
    float soilMoisture1Offset;
    float soilMoisture2Offset;
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
    float convertSoilMoistureToPercentage(float rawValue, int sensor);
    bool validateTemperature(float temp);
    bool validateHumidity(float humidity);

public:
    SensorManager();
    ~SensorManager();
    
    /**
     * @brief Initialize all sensors and calibration data
     * @return true if all sensors initialized successfully
     */
    bool begin();
    
    /**
     * @brief Read all sensors and perform validation
     * @return true if at least one sensor reading was successful
     */
    bool readSensors();
    
    /**
     * @brief Get most recent sensor readings (may include invalid data)
     * @return SensorData struct with current readings
     */
    SensorData getCurrentData();
    
    /**
     * @brief Get last known valid sensor readings
     * @return SensorData struct with validated readings
     */
    SensorData getLastValidData();
    
    /**
     * @brief Calibrate soil moisture sensor offset
     * @param sensor Sensor number (1 or 2)
     * @param offset Calibration offset value
     */
    void setSoilMoistureOffset(int sensor, float offset);
    
    /**
     * @brief Check if sensor data passes all validation criteria
     * @param data SensorData to validate
     * @return true if data is valid and trustworthy
     */
    bool isDataValid(const SensorData& data);
    
    /**
     * @brief Get descriptive error message for last operation
     * @return String containing error description
     */
    String getLastError();
    
    // Sensor health monitoring
    int getTempErrors() const { return consecutiveTempErrors; }
    int getHumidityErrors() const { return consecutiveHumidityErrors; }
    
    // Simplified stubs for compatibility
    bool updateSoilSampling();
    SystemStats getStatistics();
    void resetStatistics();
    void updateStatistics(const SensorData& data);
};

extern SensorManager sensors;

#endif
