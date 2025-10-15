#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"
#include <DHT.h>

class SensorManager {
private:
    DHT* dht;
    unsigned long lastReadTime;
    float soilMoisture1Offset;
    float soilMoisture2Offset;
    int readingIndex;
    bool bufferFull;
    bool lastDhtValid;
    bool lastSoilComplete;
    SensorData currentData;
    SensorData lastValidData;

    float readSoilMoisture(int pin);
    float convertSoilMoistureToPercentage(float rawValue, int sensor);

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
    
    // Simplified stubs for compatibility
    bool updateSoilSampling();
    SystemStats getStatistics();
    void resetStatistics();
    void updateStatistics(const SensorData& data);
};

extern SensorManager sensors;

#endif
