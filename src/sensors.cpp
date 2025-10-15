#include "sensors.h"
#include "system.h"
// External temperature sensors removed

SensorManager sensors;

SensorManager::SensorManager() {
    dht = nullptr;
    // External temperature sensor pointers removed
    
    lastReadTime = 0;
    soilMoisture1Offset = 0.0;
    soilMoisture2Offset = 0.0;
    readingIndex = 0;
    bufferFull = false;
    
    // Inicializar datos
    currentData = {0.0, 0.0, 0.0, 0.0, 0, false};
    lastValidData = currentData;
}

SensorManager::~SensorManager() {
    if (dht) delete dht;
    // External temperature sensor clean-up removed
}

bool SensorManager::begin() {
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("Initializing sensors..."));
#endif
    
    // Inicializar sensor DHT11
    dht = new DHT(DHT_PIN, DHT_TYPE);
    dht->begin();
    // Tiempo de estabilización inicial completo
    delay(dhtCurrentStabilizeMs);
    
    // NTC/DS18B20 removed
    
    // Configurar pines ADC para sensores de humedad de suelo
    analogReadResolution(12); // 12-bit ADC
    analogSetAttenuation(ADC_11db); // Para el rango completo de voltaje
    
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("Sensors initialized successfully"));
#endif
    
    // Intento lectura inicial adicional rápida para poblar datos (1/4 del tiempo)
    delay(dhtCurrentStabilizeMs / 4);
    if (readSensors()) {
        // Lectura inicial exitosa (detalles omitidos para reducir ruido de log)
        return true;
    } else {
#if MIN_LOG_LEVEL <= LOG_WARNING
    DEBUG_PRINTLN(String(F("Initial sensor reading failed: ")) + lastError);
#endif
        return false;
    }
}

bool SensorManager::readSensors() {
    SensorData newData;
    newData.timestamp = system_manager.getCurrentTimestamp();
    newData.valid = false; // se establecerá en base al DHT (opción B)
    lastSoilComplete = false; // se recalcula cada ciclo
    lastDhtValid = false;
    
    // Leer DHT (temperatura y humedad ambiental)
    float temp = dht->readTemperature();
    float humidity = dht->readHumidity();
    
    if (isnan(temp) || isnan(humidity)) {
        // Diagnóstico extendido solo cuando el sensor sigue fallando
        lastError = "DHT reading failed - using last valid data";
#if MIN_LOG_LEVEL <= LOG_WARNING
        static uint8_t dhtFailCount = 0;
        dhtFailCount++;
        // Tolerancia aumentada: solo reportar tras 20 fallos consecutivos (antes 5)
        if (dhtFailCount <= 20 || (dhtFailCount % 20) == 0) {
            DEBUG_PRINT(F("[DHT11][WARN] Lectura invalida (NaN). Intento="));
            DEBUG_PRINT(dhtFailCount);
            DEBUG_PRINT(F(" stabilize_ms="));
            DEBUG_PRINT(dhtCurrentStabilizeMs);
            DEBUG_PRINT(F(" usando previos T="));
            DEBUG_PRINT(lastValidData.temperature, 1);
            DEBUG_PRINT(F("C H="));
            DEBUG_PRINT(lastValidData.humidity, 1);
            DEBUG_PRINTLN(F("%"));
        }
#endif
        newData.temperature = lastValidData.temperature;
        newData.humidity = lastValidData.humidity;
        // Marcar como válido si tenemos datos previos válidos (tolerancia extendida)
        if (lastValidData.valid) {
            newData.valid = true;
            lastDhtValid = true;
        }
        // En caso de fallo, aumentar ligeramente tiempo de estabilización (hasta el máximo base)
        if (dhtCurrentStabilizeMs < DHT_STABILIZE_MS) {
            dhtCurrentStabilizeMs = (uint16_t)std::min<int>(DHT_STABILIZE_MS, dhtCurrentStabilizeMs + 200);
        }
    } else {
        // Reset contador de fallos
#if MIN_LOG_LEVEL <= LOG_WARNING
        static uint8_t dhtFailCount = 0;
        if (dhtFailCount > 0) {
            DEBUG_PRINT(F("[DHT] Recuperado tras "));
            DEBUG_PRINT(dhtFailCount);
            DEBUG_PRINTLN(F(" fallos"));
            dhtFailCount = 0;
        }
#endif
        newData.temperature = temp;
        newData.humidity = humidity;
#if MIN_LOG_LEVEL <= LOG_INFO
        DEBUG_PRINT(F("[DHT] OK T="));
        DEBUG_PRINT(newData.temperature, 1);
        DEBUG_PRINT(F("C H="));
        DEBUG_PRINT(newData.humidity, 1);
        DEBUG_PRINT(F("% stabilize_ms="));
        DEBUG_PRINTLN(dhtCurrentStabilizeMs);
#endif
        if (!dhtFirstSuccess) {
            dhtFirstSuccess = true;
            // Reducir a la mitad inicial tras primer éxito
            dhtCurrentStabilizeMs = (uint16_t)std::max<int>(DHT_STABILIZE_MIN_MS, (int)(dhtCurrentStabilizeMs * DHT_STABILIZE_DECAY_FACTOR));
        } else {
            // Decaimos gradualmente hacia el mínimo
            if (dhtCurrentStabilizeMs > DHT_STABILIZE_MIN_MS) {
                dhtCurrentStabilizeMs = (uint16_t)std::max<int>(DHT_STABILIZE_MIN_MS, (int)(dhtCurrentStabilizeMs * DHT_STABILIZE_DECAY_FACTOR));
            }
        }
        lastDhtValid = true;
        newData.valid = true; // Opción B: 'valid' refleja sólo estado DHT
    }
    
    // Conversión DS18B20 (omitida si deshabilitado)
    // External temperature acquisition removed
    
    // Si hay un muestreo no bloqueante en curso, intentar completarlo; si no, iniciar
    if (!soilState.active) {
        soilState.active = true;
        soilState.sensorIndex = 0;
        soilState.samplesCollected = 0;
        soilState.accumulator = 0;
        soilState.pin = SOIL_MOISTURE_1_PIN;
        soilState.lastSampleMillis = 0; // Forzar sample inmediata
    }
    // Ejecutar hasta terminar (fallback bloqueante con yield cooperativo)
    unsigned long start = millis();
    while (soilState.active && (millis() - start) < 20) { // Limitar ocupación CPU
        updateSoilSampling();
        delay(0); // ceder al scheduler
    }
    if (!soilState.active) {
        // Lecturas completadas y almacenadas en currentData (o campos temporales); usar lastValidData si no
        newData.soil_moisture_1 = currentData.soil_moisture_1;
        newData.soil_moisture_2 = currentData.soil_moisture_2;
        lastSoilComplete = true;
    } else {
        // Aún no listo: reutilizar última válida y marcar valid=false parcial (no abortar resto de sensores)
        newData.soil_moisture_1 = lastValidData.soil_moisture_1;
        newData.soil_moisture_2 = lastValidData.soil_moisture_2;
        // No alteramos newData.valid (DHT-centric)
    }
    
    // Validar datos
    if (!isDataValid(newData)) {
        lastError = "Sensor data validation failed";
        // No abortar completamente: mantener lastValidData y reportar false
        return false;
    }
    
    // Actualizar datos actuales
    currentData = newData;
    if (newData.valid) {
        lastValidData = newData;
    }
    lastReadTime = millis();
    
    // Actualizar estadísticas
    updateStatistics(newData);
    
    return true;
}

float SensorManager::readSoilMoisture(int pin) {
    // Leer múltiples muestras para obtener un promedio más estable
    long total = 0;
    for (int i = 0; i < SOIL_MOISTURE_SAMPLES; i++) {
        total += analogRead(pin);
        delay(10);
    }
    
    float average = total / (float)SOIL_MOISTURE_SAMPLES;
    
    // Convertir a porcentaje según el sensor (1 o 2)
    if (pin == SOIL_MOISTURE_1_PIN) {
        return convertSoilMoistureToPercentage(average, 1);
    } else {
        return convertSoilMoistureToPercentage(average, 2);
    }
}

bool SensorManager::updateSoilSampling() {
    if (!soilState.active) return false;
    unsigned long now = millis();
    if (soilState.lastSampleMillis != 0 && (now - soilState.lastSampleMillis) < SOIL_SAMPLE_INTERVAL_MS) {
        return false; // Esperar próximo intervalo
    }
    soilState.lastSampleMillis = now;
    int raw = analogRead(soilState.pin);
    soilState.accumulator += raw;
    soilState.samplesCollected++;
    if (soilState.samplesCollected >= SOIL_MOISTURE_SAMPLES) {
        float avg = soilState.accumulator / (float)SOIL_MOISTURE_SAMPLES;
        if (soilState.sensorIndex == 0) {
            currentData.soil_moisture_1 = convertSoilMoistureToPercentage(avg, 1);
            // Preparar segundo sensor
            soilState.sensorIndex = 1;
            soilState.samplesCollected = 0;
            soilState.accumulator = 0;
            soilState.pin = SOIL_MOISTURE_2_PIN;
            soilState.lastSampleMillis = 0; // inmediato
        } else {
            currentData.soil_moisture_2 = convertSoilMoistureToPercentage(avg, 2);
            soilState.active = false; // Completado
        }
    }
    return !soilState.active;
}

float SensorManager::convertSoilMoistureToPercentage(float rawValue, int sensor) {
    // Convertir valor ADC a porcentaje de humedad
    // Valores más altos = más seco, valores más bajos = más húmedo
    float percentage = map(rawValue, SOIL_MOISTURE_DRY_VALUE, SOIL_MOISTURE_WET_VALUE, 0, 100);
    
    // Aplicar offset de calibración
    if (sensor == 1) {
        percentage += soilMoisture1Offset;
    } else {
        percentage += soilMoisture2Offset;
    }
    
    // Limitar entre 0 y 100
    percentage = constrain(percentage, 0.0, 100.0);
    
    return percentage;
}

bool SensorManager::isDataValid(const SensorData& data) {
    // Verificar rangos razonables
    if (data.temperature < -40.0 || data.temperature > 80.0) return false;
    if (data.humidity < 0.0 || data.humidity > 100.0) return false;
    if (data.soil_moisture_1 < 0.0 || data.soil_moisture_1 > 100.0) return false;
    if (data.soil_moisture_2 < 0.0 || data.soil_moisture_2 > 100.0) return false;
    // External temperature validation removed
    
    return true;
}

void SensorManager::updateStatistics(const SensorData& data) {
    if (!data.valid) return;
    
    // Agregar datos al buffer circular
    tempReadings[readingIndex] = data.temperature;
    humidityReadings[readingIndex] = data.humidity;
    soilReadings[readingIndex] = (data.soil_moisture_1 + data.soil_moisture_2) / 2.0;
    
    readingIndex = (readingIndex + 1) % 100;
    if (readingIndex == 0) {
        bufferFull = true;
    }
}

SystemStats SensorManager::getStatistics() {
    SystemStats stats = {0};
    
    if (readingIndex == 0 && !bufferFull) {
        return stats; // No hay datos
    }
    
    int count = bufferFull ? 100 : readingIndex;
    
    // Calcular estadísticas de temperatura
    float tempSum = 0;
    stats.temp_min = tempReadings[0];
    stats.temp_max = tempReadings[0];
    
    for (int i = 0; i < count; i++) {
        tempSum += tempReadings[i];
        if (tempReadings[i] < stats.temp_min) stats.temp_min = tempReadings[i];
        if (tempReadings[i] > stats.temp_max) stats.temp_max = tempReadings[i];
    }
    stats.temp_avg = tempSum / count;
    
    // Calcular estadísticas de humedad
    float humiditySum = 0;
    stats.humidity_min = humidityReadings[0];
    stats.humidity_max = humidityReadings[0];
    
    for (int i = 0; i < count; i++) {
        humiditySum += humidityReadings[i];
        if (humidityReadings[i] < stats.humidity_min) stats.humidity_min = humidityReadings[i];
        if (humidityReadings[i] > stats.humidity_max) stats.humidity_max = humidityReadings[i];
    }
    stats.humidity_avg = humiditySum / count;
    
    // Calcular estadísticas de humedad del suelo
    float soilSum = 0;
    stats.soil_min = soilReadings[0];
    stats.soil_max = soilReadings[0];
    
    for (int i = 0; i < count; i++) {
        soilSum += soilReadings[i];
        if (soilReadings[i] < stats.soil_min) stats.soil_min = soilReadings[i];
        if (soilReadings[i] > stats.soil_max) stats.soil_max = soilReadings[i];
    }
    stats.soil_avg = soilSum / count;
    
    stats.uptime = system_manager.getUptime();
    
    return stats;
}

void SensorManager::resetStatistics() {
    readingIndex = 0;
    bufferFull = false;
    memset(tempReadings, 0, sizeof(tempReadings));
    memset(humidityReadings, 0, sizeof(humidityReadings));
    memset(soilReadings, 0, sizeof(soilReadings));
}

void SensorManager::setSoilMoistureOffset(int sensor, float offset) {
    if (sensor == 1) {
        soilMoisture1Offset = offset;
    #if VERBOSE_LOGS
    DEBUG_PRINTF("Soil1 off=%.2f\n", offset);
    #endif
    } else if (sensor == 2) {
        soilMoisture2Offset = offset;
    #if VERBOSE_LOGS
    DEBUG_PRINTF("Soil2 off=%.2f\n", offset);
    #endif
    }
}

SensorData SensorManager::getCurrentData() {
    return currentData;
}

SensorData SensorManager::getLastValidData() {
    return lastValidData;
}

String SensorManager::getLastError() {
    return lastError;
}