#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"
#include <DHT.h>
#include <memory>

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

    float readSoilMoisture(int pin);
    float convertSoilMoistureToPercentage(float rawValue, int sensor);
    bool validateTemperature(float temp);
    bool validateHumidity(float humidity);

public:
    SensorManager();
    ~SensorManager();
    
    bool begin();
    bool readSensors();
    SensorData getCurrentData();
    SensorData getLastValidData();
    void setSoilMoistureOffset(int sensor, float offset);
    bool isDataValid(const SensorData& data);
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
