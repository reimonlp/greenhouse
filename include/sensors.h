#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"
#include <DHT.h>
// External temperature sensors (NTC/DS18B20) removed

class SensorManager {
private:
    DHT* dht;
    // External temperature sensor members removed
    
    SensorData currentData;
    SensorData lastValidData;
    unsigned long lastReadTime;
    // S3: estabilización adaptativa DHT
    uint16_t dhtCurrentStabilizeMs = DHT_STABILIZE_MS;
    bool dhtFirstSuccess = false;
    // Estado conversión no bloqueante DS18B20
    bool ds18InProgress = false;
    unsigned long ds18StartMs = 0;
    uint16_t ds18ConversionMs = 750; // se ajusta según resolución
    // Estado para muestreo no bloqueante de humedad de suelo (S2)
    struct SoilSampleState {
        bool active = false;
        uint8_t sensorIndex = 0;      // 0 -> sensor1, 1 -> sensor2
        uint16_t samplesCollected = 0;
        uint32_t accumulator = 0;
        unsigned long lastSampleMillis = 0;
        int pin = -1;
    } soilState;
    
    // Calibración de sensores de humedad de suelo
    float soilMoisture1Offset;
    float soilMoisture2Offset;
    
    // Estadísticas
    float tempReadings[100];
    float humidityReadings[100];
    float soilReadings[100];
    int readingIndex;
    bool bufferFull;
    // Flags de validez por subsistema
    bool lastDhtValid = false;
    bool lastSoilComplete = false;
    // External temperature completeness removed
    
public:
    SensorManager();
    ~SensorManager();
    
    bool begin();
    bool readSensors();
    bool isDs18InProgress() const { return ds18InProgress; }
    uint16_t getCurrentDhtStabilizeMs() const { return dhtCurrentStabilizeMs; }
    bool isDhtValid() const { return lastDhtValid; }
    bool isSoilComplete() const { return lastSoilComplete; }
    // NTC completeness removed
    // Nueva versión incremental no bloqueante; devuelve true cuando se completó la adquisición de ambos sensores
    bool updateSoilSampling();
    SensorData getCurrentData();
    SensorData getLastValidData();
    
    // Calibración (offset ajustable vía API)
    void setSoilMoistureOffset(int sensor, float offset);
    
    // Estadísticas
    SystemStats getStatistics();
    void resetStatistics();
    
    // Validación
    bool isDataValid(const SensorData& data);
    String getLastError();
    
private:
    float readSoilMoisture(int pin);
    float convertSoilMoistureToPercentage(float rawValue, int sensor);
    void updateStatistics(const SensorData& data);
    String lastError;
};

extern SensorManager sensors;

#endif // SENSORS_H