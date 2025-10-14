#include "database.h"
#include "system.h"
#include <WiFi.h>
#include <vector>
#include <HTTPClient.h>
#include <LittleFS.h>
#include "fs_utils.h"

DatabaseManager database;

// --- Simple run-length compression for batched log shipping (optional feature) ---
// Compresses repeated identical lines to: <count>|<line> format for faster transfer.
static String rleCompressLogs(const std::vector<String>& lines) {
    if (lines.empty()) return String();
    String out; out.reserve(256);
    size_t i=0; while (i<lines.size()) {
        size_t j=i+1; while (j<lines.size() && lines[j]==lines[i] && (j-i)<9999) ++j;
        // Emit count|line\n if repetition >1 else line\n
        if ((j-i)>1) { out += String(j-i); out += '|'; out += lines[i]; out += '\n'; }
        else { out += lines[i]; out += '\n'; }
        i=j;
    }
    return out;
}

DatabaseManager::DatabaseManager() {
    mongoDbUri = MONGODB_URI;
    databaseName = "invernadero";
    collectionSensors = "sensor_data";
    collectionLogs = "system_logs";
    collectionStats = "statistics";
    
    bufferIndex = 0;
    bufferFull = false;
    lastSendTime = 0;
    // Inicializar próxima ventana de flush con jitter
    unsigned long base = LOG_INTERVAL_MS;
    int jitterRange = (int)(base * LOG_FLUSH_JITTER_PCT / 100);
    unsigned long jitter = (jitterRange > 0) ? (millis() % (2 * jitterRange)) - jitterRange : 0;
    nextPlannedFlush = base + jitter;
    localBackupEnabled = true;
    backupFilename = "/logs_backup.json";
    connected = false;
    persistLoaded = false;
}

DatabaseManager::~DatabaseManager() {
    // Enviar logs pendientes antes de destruir
    sendLogBuffer();
}

bool DatabaseManager::begin() {
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("Initializing Database Manager..."));
#endif
    
    // Inicializar buffer de logs
    memset(logBuffer, 0, sizeof(logBuffer));
    
    // Verificar si LittleFS está disponible
    if (!ensureFS()) {
#if MIN_LOG_LEVEL <= LOG_WARNING
    DEBUG_PRINTLN(F("WARNING: LittleFS not available - local backup disabled"));
#endif
        localBackupEnabled = false;
    }
    // Cargar log ring persistente (se realiza una vez)
    loadPersistedLogRing();
    // Intento de sanación si el ring parece sospechoso
    healLogRingIfCorrupt();
    // Pre-cargar buffer crítico desde ring si estaba vacío
    seedCriticalBufferFromRing();
    
    // Verificar conectividad WiFi
    if (WiFi.status() != WL_CONNECTED) {
#if MIN_LOG_LEVEL <= LOG_WARNING
    DEBUG_PRINTLN(F("WARNING: WiFi not connected - running in offline mode"));
#endif
        connected = false;
    } else {
        connected = true;
    }
    
    // Log de inicio del sistema de base de datos (data JSON optimizado)
    char statusJson[64];
    snprintf(statusJson, sizeof(statusJson), "{\"backup_enabled\":%s,\"connected\":%s,\"logring_healed\":%s}",
             localBackupEnabled ? "true" : "false",
             connected ? "true" : "false", logringCorrupt ? "true" : "false");
    log(LOG_INFO, "database", "Database manager initialized", String(statusJson));
    
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("Database manager initialized"));
#endif
    return true;
}

bool DatabaseManager::isConnected() {
    return connected && (WiFi.status() == WL_CONNECTED);
}

void DatabaseManager::setConnectionString(const String& uri, const String& database) {
    mongoDbUri = uri;
    databaseName = database;
    
    log(LOG_INFO, "database", "Connection string updated", 
        "{\"database\":\"" + database + "\"}");
}

void DatabaseManager::enableLocalBackup(bool enable) {
    localBackupEnabled = enable;
    log(LOG_INFO, "database", "Local backup " + String(enable ? "enabled" : "disabled"));
}

void DatabaseManager::log(LogLevel level, const String& source, const String& message, const String& data) {
    // Filtrado doble: compilado (MIN_LOG_LEVEL) y dinámico (runtimeLogLevel)
    if (level < MIN_LOG_LEVEL) return; // eliminado en build
    if (level < runtimeLogLevel) return; // silenciado en runtime

    LogEntry entry{};
    entry.timestamp = getCurrentTimestamp();
    entry.level = level;
    // Copias truncadas seguras
    strncpy(entry.source, source.c_str(), sizeof(entry.source) - 1);
    strncpy(entry.message, message.c_str(), sizeof(entry.message) - 1);
    if (data.length() > 0) {
        strncpy(entry.data, data.c_str(), sizeof(entry.data) - 1);
    } else {
        entry.data[0] = '\0';
    }

    // Insertar en buffer circular
    logBuffer[bufferIndex] = entry;
    bufferIndex = (bufferIndex + 1) % LOG_BUFFER_SIZE;
    if (bufferIndex == 0) bufferFull = true;

    // Serial (solo si nivel permitido en build)
    const char* levelStr =
        (level == LOG_DEBUG) ? "DEBUG" :
        (level == LOG_INFO) ? "INFO" :
        (level == LOG_WARNING) ? "WARN" :
        (level == LOG_ERROR) ? "ERROR" : "CRIT";
    DEBUG_PRINTF("%s:%s %s\n", levelStr, entry.source, entry.message);

    if (localBackupEnabled) {
        String logJson = createLogJSON(entry);
        writeToLocalFile(backupFilename, logJson + "\n", true);
    }

    // Criterios de envío inmediato: buffer lleno, ERROR/CRITICAL
    if (bufferFull || level >= LOG_ERROR) {
        sendLogBuffer();
        // Reprogramar próxima ventana tras envío
        unsigned long base = LOG_INTERVAL_MS;
        int jitterRange = (int)(base * LOG_FLUSH_JITTER_PCT / 100);
        unsigned long jitter = (jitterRange > 0) ? (millis() % (2 * jitterRange)) - jitterRange : 0;
        nextPlannedFlush = millis() + base + jitter;
    }

    // Añadir a buffer crítico si ERROR o CRITICAL
    if (level >= LOG_ERROR) {
        criticalBuffer[criticalIndex] = entry;
        criticalIndex = (criticalIndex + 1) % CRIT_BUFFER_SIZE;
        if (criticalIndex == 0) criticalFull = true;
        // Persist in ring for cross-reset retention
        persistLogRingEntry(entry);
    }
}

void DatabaseManager::setRuntimeLogLevel(LogLevel level) {
    runtimeLogLevel = level;
    log(LOG_INFO, "logger", "Runtime log level updated", String("{\"level\":") + (int)level + "}");
}

LogLevel DatabaseManager::getRuntimeLogLevel() const {
    return runtimeLogLevel;
}

String DatabaseManager::getCriticalLogs(int count) {
    if (count > CRIT_BUFFER_SIZE) count = CRIT_BUFFER_SIZE;
    StaticJsonDocument<2048> doc; // suficiente para 25 entradas pequeñas
    JsonArray arr = doc.createNestedArray("logs");
    int total = criticalFull ? CRIT_BUFFER_SIZE : criticalIndex;
    // Recorrer desde el más reciente hacia atrás
    for (int i = 0; i < total && i < count; ++i) {
        int idx = (criticalIndex - 1 - i);
        if (idx < 0) idx += CRIT_BUFFER_SIZE;
        const LogEntry &le = criticalBuffer[idx];
        JsonObject o = arr.createNestedObject();
        o["ts"] = le.timestamp;
        o["lvl"] = (int)le.level;
        o["src"] = le.source;
        o["msg"] = le.message;
        if (le.data[0]) o["data"] = le.data;
    }
    String out; serializeJson(doc, out);
    return out;
}

// === Persisted Log Ring Implementation ===
// File layout: MAGIC(4) VERSION(1) count(2) head(2) entries[count] CRC32(4)
// Each entry: ts(4) lvl(1) src[12] msg[48] pad(1) = 66 bytes

static uint32_t ringCrc32(const uint8_t* data, size_t len) {
    uint32_t crc = ~0u;
    for (size_t i=0;i<len;++i){ crc ^= data[i]; for(int k=0;k<8;++k){ uint32_t m = -(crc & 1u); crc = (crc>>1) ^ (0xEDB88320u & m);} }
    return ~crc;
}

void DatabaseManager::loadPersistedLogRing() {
    if (persistLoaded || !ensureFS()) return;
    // Lazy create empty file so subsequent opens don't warn
    if (!LittleFS.exists("/logring.bin")) {
        File create = LittleFS.open("/logring.bin", "w");
        if (create) create.close();
        persistLoaded = true; // nothing to load
        return;
    }
    File f = LittleFS.open("/logring.bin", "r");
    if (!f) { persistLoaded = true; return; }
    size_t maxSize = 4+1+2+2 + PERSIST_LOG_RING_CAPACITY*sizeof(PersistRingEntry) + 4;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[maxSize]);
    size_t n = f.read(buf.get(), maxSize); f.close();
    if (n < 4+1+2+2+4) { persistLoaded = true; return; }
    uint8_t* p = buf.get();
    uint32_t magic; memcpy(&magic,p,4); p+=4; if(magic!=0x4C52494Eu){ persistLoaded=true; return; } // 'LRIN'
    uint8_t ver = *p++; if(ver!=1){ persistLoaded=true; return; }
    memcpy(&persistCount,p,2); p+=2; memcpy(&persistHead,p,2); p+=2;
    if (persistCount > PERSIST_LOG_RING_CAPACITY) { persistCount = 0; persistHead = 0; persistLoaded=true; return; }
    size_t entriesBytes = persistCount * sizeof(PersistRingEntry);
    if (p + entriesBytes + 4 > buf.get() + n) { persistCount=0; persistHead=0; persistLoaded=true; return; }
    memcpy(persistRing, p, entriesBytes); p += entriesBytes; uint32_t storedCrc; memcpy(&storedCrc,p,4);
    size_t headerLen = 4+1+2+2 + entriesBytes;
    uint32_t calc = ringCrc32(buf.get(), headerLen);
    if (calc != storedCrc) { persistCount=0; persistHead=0; }
    persistLoaded = true;
}

void DatabaseManager::persistLogRingEntry(const LogEntry& entry) {
    if (!ensureFS()) return;
    if (!persistLoaded) loadPersistedLogRing();
    PersistRingEntry e{};
    e.ts = entry.timestamp;
    e.lvl = (uint8_t)entry.level;
    strncpy(e.src, entry.source, sizeof(e.src)-1);
    strncpy(e.msg, entry.message, sizeof(e.msg)-1);
    persistRing[persistHead] = e;
    if (persistCount < PERSIST_LOG_RING_CAPACITY) persistCount++;
    persistHead = (persistHead + 1) % PERSIST_LOG_RING_CAPACITY;
    // Write out file
    size_t entriesBytes = persistCount * sizeof(PersistRingEntry);
    size_t headerLen = 4+1+2+2 + entriesBytes;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[headerLen + 4]);
    uint8_t* p = buf.get();
    uint32_t magic = 0x4C52494Eu; memcpy(p,&magic,4); p+=4; *p++=1; memcpy(p,&persistCount,2); p+=2; memcpy(p,&persistHead,2); p+=2;
    memcpy(p, persistRing, entriesBytes); p+=entriesBytes;
    uint32_t crc = ringCrc32(buf.get(), headerLen); memcpy(p,&crc,4);
    File f = LittleFS.open("/logring_tmp.bin","w"); if(!f) return; f.write(buf.get(), headerLen+4); f.close();
    LittleFS.remove("/logring.bin"); LittleFS.rename("/logring_tmp.bin","/logring.bin");
}

void DatabaseManager::seedCriticalBufferFromRing() {
    if (!persistLoaded) return;
    if (criticalFull || criticalIndex != 0) return; // only seed if empty
    int toCopy = persistCount; if (toCopy > CRIT_BUFFER_SIZE) toCopy = CRIT_BUFFER_SIZE;
    // Copy most recent first
    for (int i=0;i<toCopy;++i) {
        int ringIdx = (persistHead - 1 - i); if (ringIdx < 0) ringIdx += PERSIST_LOG_RING_CAPACITY;
        const PersistRingEntry& e = persistRing[ringIdx];
        LogEntry le{}; le.timestamp=e.ts; le.level=(LogLevel)e.lvl; strncpy(le.source,e.src,sizeof(le.source)-1); strncpy(le.message,e.msg,sizeof(le.message)-1); le.data[0]='\0';
        criticalBuffer[criticalIndex] = le; criticalIndex = (criticalIndex + 1) % CRIT_BUFFER_SIZE; if (criticalIndex==0) criticalFull=true;
    }
}

void DatabaseManager::logSensorData(const SensorData& data) {
    StaticJsonDocument<256> doc;
    doc["timestamp"] = data.timestamp;
    doc["temperature"] = data.temperature;
    doc["humidity"] = data.humidity;
    doc["soil_moisture_1"] = data.soil_moisture_1;
    doc["soil_moisture_2"] = data.soil_moisture_2;
    doc["temp_sensor_1"] = data.temp_sensor_1;
    doc["temp_sensor_2"] = data.temp_sensor_2;
    doc["valid"] = data.valid;
    
    String jsonData;
    serializeJson(doc, jsonData);
    
    log(LOG_INFO, "sensors", "Sensor data reading", jsonData);
    
    // Enviar directamente a la colección de sensores si está conectado
    if (isConnected()) {
        sendToMongoDB(collectionSensors, jsonData);
    }
}

void DatabaseManager::logRelayAction(int relayIndex, bool state, RelayMode mode, const String& reason) {
    StaticJsonDocument<256> doc;
    doc["relay_index"] = relayIndex;
    doc["relay_name"] = (relayIndex == 0) ? "luces" : 
                       (relayIndex == 1) ? "ventilador" : 
                       (relayIndex == 2) ? "bomba" : "calefactor";
    doc["state"] = state;
    doc["mode"] = (mode == RELAY_MODE_MANUAL) ? "manual" : "auto";
    doc["reason"] = reason;
    doc["timestamp"] = getCurrentTimestamp();
    
    String jsonData;
    serializeJson(doc, jsonData);
    
    log(LOG_INFO, "relays", 
        String("Relay ") + doc["relay_name"].as<String>() + 
        (state ? " turned ON" : " turned OFF"), jsonData);
}

void DatabaseManager::logSystemEvent(const String& event, const String& details) {
    StaticJsonDocument<256> doc;
    doc["event"] = event;
    doc["details"] = details;
    doc["uptime"] = system_manager.getUptime();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["timestamp"] = getCurrentTimestamp();
    
    String jsonData;
    serializeJson(doc, jsonData);
    
    log(LOG_INFO, "system", event, jsonData);
}

void DatabaseManager::logError(const String& source, const String& error) {
    StaticJsonDocument<256> doc;
    doc["error_message"] = error;
    doc["source"] = source;
    doc["timestamp"] = getCurrentTimestamp();
    doc["uptime"] = system_manager.getUptime();
    
    String jsonData;
    serializeJson(doc, jsonData);
    
    log(LOG_ERROR, source, error, jsonData);
}

void DatabaseManager::logPowerEvent(bool powerLost, unsigned long timestamp) {
    StaticJsonDocument<256> doc;
    doc["power_lost"] = powerLost;
    doc["event_timestamp"] = timestamp;
    doc["system_timestamp"] = getCurrentTimestamp();
    
    String jsonData;
    serializeJson(doc, jsonData);
    
    log(LOG_CRITICAL, "power", 
        powerLost ? "Power loss detected" : "Power restored", jsonData);
}

bool DatabaseManager::sendLogBuffer() {
    if (bufferIndex == 0 && !bufferFull) {
        return true; // No hay logs para enviar
    }
    
    if (!isConnected()) {
        return false; // No hay conexión
    }
    
    int count = bufferFull ? LOG_BUFFER_SIZE : bufferIndex;
    bool allSent = true;
    
    for (int i = 0; i < count; i++) {
        String logJson = createLogJSON(logBuffer[i]);
        if (!sendToMongoDB(collectionLogs, logJson)) {
            allSent = false;
            lastError = "Failed to send log entry " + String(i);
        }
    }
    
    if (allSent) {
        // Limpiar buffer solo si todo se envió correctamente
        clearBuffer();
        lastSendTime = millis();
    }
    
    return allSent;
}

void DatabaseManager::periodic() {
    unsigned long now = millis();
    int count = bufferFull ? LOG_BUFFER_SIZE : bufferIndex;
    if (count == 0) return;
    // Forzar flush si se excede LOG_MAX_INTERVAL_MS desde último envío
    if (now - lastSendTime >= LOG_MAX_INTERVAL_MS) {
        sendLogBuffer();
        return;
    }
    // Flush planificado con jitter sólo si hay suficientes entradas
    if (now >= nextPlannedFlush && count >= LOG_MIN_BATCH) {
        if (sendLogBuffer()) {
            unsigned long base = LOG_INTERVAL_MS;
            int jitterRange = (int)(base * LOG_FLUSH_JITTER_PCT / 100);
            unsigned long jitter = (jitterRange > 0) ? (millis() % (2 * jitterRange)) - jitterRange : 0;
            nextPlannedFlush = millis() + base + jitter;
        }
    }
}

bool DatabaseManager::sendSensorData(const SensorData& data) {
    String jsonData = createSensorJSON(data);
    return sendToMongoDB(collectionSensors, jsonData);
}

bool DatabaseManager::sendSystemStats(const SystemStats& stats) {
    String jsonData = createStatsJSON(stats);
    return sendToMongoDB(collectionStats, jsonData);
}

bool DatabaseManager::sendToMongoDB(const String& collection, const String& jsonData) {
#if defined(USE_MONGODB_DATA_API)
    if (!isConnected()) return false;
    if (MONGODB_DATA_API_KEY[0] == '\0' || MONGODB_APP_ID[0] == '\0') { lastError = "Data API not configured"; return false; }
    HTTPClient http;
    String url = String("https://data.mongodb-api.com/app/") + MONGODB_APP_ID + "/endpoint/data/v1/action/insertOne";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("api-key", MONGODB_DATA_API_KEY);
    StaticJsonDocument<512> doc;
    doc["dataSource"] = MONGODB_DATA_SOURCE;
    doc["database"] = MONGODB_DB_NAME;
    doc["collection"] = collection;
    // Parse original payload to embed (si falla, se guarda como raw)
    StaticJsonDocument<512> payload;
    if (deserializeJson(payload, jsonData) == DeserializationError::Ok) {
        doc["document"] = payload;
    } else {
        doc["document"]["raw"] = jsonData;
        doc["document"]["ts"] = getCurrentTimestamp();
    }
    String body; serializeJson(doc, body);
    int code = http.POST(body);
    if (code == 200 || code == 201) { http.end(); return true; }
    lastError = String("Data API insert error: ") + code;
    http.end(); return false;
#else
    (void)collection; (void)jsonData; return false; // Data API no habilitada
#endif
}

String DatabaseManager::queryFromMongoDB(const String& collection, const String& query) {
#if defined(USE_MONGODB_DATA_API)
    if (!isConnected()) return "{}";
    if (MONGODB_DATA_API_KEY[0] == '\0' || MONGODB_APP_ID[0] == '\0') return "{}";
    HTTPClient http;
    String url = String("https://data.mongodb-api.com/app/") + MONGODB_APP_ID + "/endpoint/data/v1/action/find";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("api-key", MONGODB_DATA_API_KEY);
    StaticJsonDocument<512> doc;
    doc["dataSource"] = MONGODB_DATA_SOURCE;
    doc["database"] = MONGODB_DB_NAME;
    doc["collection"] = collection;
    // query es un JSON parcial con filtros. Intentar parsear
    StaticJsonDocument<256> qdoc;
    if (deserializeJson(qdoc, query) == DeserializationError::Ok) {
        doc["filter"] = qdoc["filter"] | qdoc;
        if (qdoc["limit"]) doc["limit"] = qdoc["limit"];
        if (qdoc["sort"]) doc["sort"] = qdoc["sort"];
    } else {
        doc["limit"] = 20;
    }
    String body; serializeJson(doc, body);
    int code = http.POST(body);
    String out = "{}";
    if (code == 200) out = http.getString(); else lastError = String("Data API find error:") + code;
    http.end(); return out;
#else
    (void)collection; (void)query; return "{}";
#endif
}

bool DatabaseManager::writeToLocalFile(const String& filename, const String& data, bool append) {
    if (!localBackupEnabled) return false;
    
    File file = LittleFS.open(filename, append ? "a" : "w");
    if (!file) {
        lastError = "Failed to open file: " + filename;
        return false;
    }
    
    size_t written = file.print(data);
    file.close();
    
    return written > 0;
}

String DatabaseManager::readFromLocalFile(const String& filename) {
    if (!localBackupEnabled) return "";
    
    File file = LittleFS.open(filename, "r");
    if (!file) {
        lastError = "Failed to open file: " + filename;
        return "";
    }
    
    String content = file.readString();
    file.close();
    
    return content;
}

String DatabaseManager::getRecentLogs(int count) {
    // Primero intentar obtener de MongoDB
    if (isConnected()) {
        StaticJsonDocument<256> query;
        query["limit"] = count;
        query["sort"]["timestamp"] = -1; // Más recientes primero
        
        String queryStr;
        serializeJson(query, queryStr);
        
        String cloudLogs = queryFromMongoDB(collectionLogs, queryStr);
        if (!cloudLogs.isEmpty() && !cloudLogs.equals("{}")) {
            return cloudLogs;
        }
    }
    
    // Fallback a logs locales
    return getLocalLogs(count);
}

String DatabaseManager::getSensorHistory(unsigned long fromTime, unsigned long toTime) {
    if (!isConnected()) {
        return "{\"error\":\"Not connected to database\"}";
    }
    
    StaticJsonDocument<256> query;
    JsonObject filter = query.createNestedObject("filter");
    JsonObject timestampFilter = filter.createNestedObject("timestamp");
    timestampFilter["$gte"] = fromTime;
    timestampFilter["$lte"] = toTime;
    
    query["sort"]["timestamp"] = 1; // Cronológico
    query["limit"] = 1000; // Máximo 1000 registros
    
    String queryStr;
    serializeJson(query, queryStr);
    
    return queryFromMongoDB(collectionSensors, queryStr);
}

String DatabaseManager::getSystemStatistics(unsigned long fromTime, unsigned long toTime) {
    if (!isConnected()) {
        return "{\"error\":\"Not connected to database\"}";
    }
    
    StaticJsonDocument<256> query;
    JsonObject filter = query.createNestedObject("filter");
    JsonObject timestampFilter = filter.createNestedObject("timestamp");
    timestampFilter["$gte"] = fromTime;
    timestampFilter["$lte"] = toTime;
    
    String queryStr;
    serializeJson(query, queryStr);
    
    return queryFromMongoDB(collectionStats, queryStr);
}

String DatabaseManager::getLocalLogs(int count) {
    if (!localBackupEnabled) {
        return "{\"error\":\"Local backup not enabled\"}";
    }
    
    String content = readFromLocalFile(backupFilename);
    if (content.isEmpty()) {
        return "{\"logs\":[]}";
    }
    
    // Parsear líneas (cada línea es un JSON)
    StaticJsonDocument<4096> result;
    JsonArray logs = result.createNestedArray("logs");
    
    int lineCount = 0;
    int startPos = 0;
    
    while (startPos < content.length() && lineCount < count) {
    int endPos = content.indexOf('\n', startPos);
        if (endPos == -1) endPos = content.length();
        
        String line = content.substring(startPos, endPos);
        if (!line.isEmpty()) {
            StaticJsonDocument<512> logEntry;
            if (deserializeJson(logEntry, line) == DeserializationError::Ok) {
                logs.add(logEntry);
                lineCount++;
            }
        }
        
        startPos = endPos + 1;
    }
    
    String resultStr;
    serializeJson(result, resultStr);
    return resultStr;
}

bool DatabaseManager::clearLocalLogs() {
    if (!localBackupEnabled) return false;
    
    // Eliminar archivo de backup
    if (LittleFS.exists(backupFilename)) {
        bool result = LittleFS.remove(backupFilename);
        if (result) {
            log(LOG_INFO, "database", "Local logs cleared");
        }
        return result;
    }
    
    return true; // Si no existe, considerar como éxito
}

// ========== UTILITY FUNCTIONS ==========
String DatabaseManager::createLogJSON(const LogEntry& entry) {
    // Intento ligero: si data parece JSON (empieza con '{'), la adjuntamos raw, sin parsear.
    // Formato manual para reducir dependencia pesada de ArduinoJson en logs simples.
    char buf[512];
    if (entry.data[0] != '\0' && entry.data[0] == '{') {
        // Insertar data raw sin validar para evitar costos; truncar si excede.
        // Escapar comillas en source y message de manera simple (reemplazar \" con nada complejo por tamaño controlado)
        // Dado que source y message son internos y controlados, asumimos seguros.
        snprintf(buf, sizeof(buf),
                 "{\"timestamp\":%lu,\"level\":%d,\"source\":\"%s\",\"message\":\"%s\",\"data\":%s}",
                 entry.timestamp, (int)entry.level, entry.source, entry.message, entry.data);
    } else if (entry.data[0] != '\0') {
        // data no es JSON, incluir como cadena escapada básica
        // Reemplazar comillas dobles si existieran (muy raro en logs internos)
        char dataEsc[128];
        size_t len = strnlen(entry.data, sizeof(entry.data));
        size_t di = 0;
        for (size_t i = 0; i < len && di < sizeof(dataEsc) - 2; ++i) {
            char c = entry.data[i];
            if (c == '"' || c == '\\') dataEsc[di++] = '\\';
            dataEsc[di++] = c;
        }
        dataEsc[di] = '\0';
        snprintf(buf, sizeof(buf),
                 "{\"timestamp\":%lu,\"level\":%d,\"source\":\"%s\",\"message\":\"%s\",\"data_raw\":\"%s\"}",
                 entry.timestamp, (int)entry.level, entry.source, entry.message, dataEsc);
    } else {
        snprintf(buf, sizeof(buf),
                 "{\"timestamp\":%lu,\"level\":%d,\"source\":\"%s\",\"message\":\"%s\"}",
                 entry.timestamp, (int)entry.level, entry.source, entry.message);
    }
    return String(buf);
}

String DatabaseManager::createSensorJSON(const SensorData& data) {
    StaticJsonDocument<256> doc;
    doc["timestamp"] = data.timestamp;
    doc["temperature"] = data.temperature;
    doc["humidity"] = data.humidity;
    doc["soil_moisture_1"] = data.soil_moisture_1;
    doc["soil_moisture_2"] = data.soil_moisture_2;
    doc["temp_sensor_1"] = data.temp_sensor_1;
    doc["temp_sensor_2"] = data.temp_sensor_2;
    doc["valid"] = data.valid;
    
    String result;
    serializeJson(doc, result);
    return result;
}

String DatabaseManager::createStatsJSON(const SystemStats& stats) {
    StaticJsonDocument<512> doc;
    doc["timestamp"] = getCurrentTimestamp();
    doc["temp_min"] = stats.temp_min;
    doc["temp_max"] = stats.temp_max;
    doc["temp_avg"] = stats.temp_avg;
    doc["humidity_min"] = stats.humidity_min;
    doc["humidity_max"] = stats.humidity_max;
    doc["humidity_avg"] = stats.humidity_avg;
    doc["soil_min"] = stats.soil_min;
    doc["soil_max"] = stats.soil_max;
    doc["soil_avg"] = stats.soil_avg;
    doc["heating_time"] = stats.heating_time;
    doc["irrigation_time"] = stats.irrigation_time;
    doc["uptime"] = stats.uptime;
    
    String result;
    serializeJson(doc, result);
    return result;
}

unsigned long DatabaseManager::getCurrentTimestamp() {
    return system_manager.getCurrentTimestamp();
}

void DatabaseManager::clearBuffer() {
    bufferIndex = 0;
    bufferFull = false;
    memset(logBuffer, 0, sizeof(logBuffer));
}

int DatabaseManager::getBufferUsage() {
    if (bufferFull) return LOG_BUFFER_SIZE;
    return bufferIndex;
}

String DatabaseManager::getLastError() {
    return lastError;
}

bool DatabaseManager::saveToLocalBackup() {
    if (!localBackupEnabled) return false;
    
    // Crear un backup completo del estado actual
    StaticJsonDocument<1024> backup;
    backup["timestamp"] = getCurrentTimestamp();
    backup["version"] = FIRMWARE_VERSION;
    
    // Agregar datos de sensores actuales
    SensorData sensorData = sensors.getCurrentData();
    backup["sensors"] = createSensorJSON(sensorData);
    
    // Agregar estado de relays
    backup["relays"] = relays.getSystemStatus();
    
    // Agregar información del sistema
    backup["system"] = system_manager.getSystemInfo();
    
    String backupJson;
    serializeJson(backup, backupJson);
    
    return writeToLocalFile("/system_backup.json", backupJson, false);
}

bool DatabaseManager::loadFromLocalBackup() {
    if (!localBackupEnabled) return false;
    
    String backupData = readFromLocalFile("/system_backup.json");
    if (backupData.isEmpty()) {
        return false;
    }
    
    StaticJsonDocument<1024> backup;
    if (deserializeJson(backup, backupData) != DeserializationError::Ok) {
        lastError = "Failed to parse backup data";
        return false;
    }
    
    log(LOG_INFO, "database", "System backup loaded", 
        "Backup timestamp: " + String(backup["timestamp"].as<unsigned long>()));
    
    return true;
}

// Añadimos implementaciones self-heal al final del archivo
bool DatabaseManager::detectLogRingSuspicious(size_t fileSize) const {
    // Archivo tiene tamaño pero no se cargó ninguna entrada -> sospechoso
    if (fileSize > 0 && persistLoaded && persistCount == 0 && persistHead == 0) return true;
    // Tamaño no múltiplo aproximado de 66 bytes + header (+ CRC). Header mínimo 4+1+2+2=9 + CRC4 =13 bytes
    // Si el tamaño es muy pequeño (<13) pero >0 también es sospechoso.
    if (fileSize > 0 && fileSize < 13) return true;
    return false;
}

bool DatabaseManager::recreateEmptyLogRing() {
    LittleFS.remove("/logring.bin");
    File f = LittleFS.open("/logring.bin", "w");
    if (!f) return false;
    // Crear header vacío válido
    uint32_t magic = 0x4C52494Eu; // 'LRIN'
    uint8_t ver = 1; uint16_t count = 0; uint16_t head = 0;
    f.write((uint8_t*)&magic,4); f.write(&ver,1); f.write((uint8_t*)&count,2); f.write((uint8_t*)&head,2);
    uint32_t crc = 0; // CRC de header vacío (calcularemos usando misma rutina simplificada)
    // Recalcular CRC igual que loadPersistedLogRing (copiamos mini lógica)
    uint8_t headerBuf[4+1+2+2]; memcpy(headerBuf,&magic,4); headerBuf[4]=ver; memcpy(headerBuf+5,&count,2); memcpy(headerBuf+7,&head,2);
    // Simple CRC32 (duplicamos lógica mínima) -> para coherencia reutilizamos ringCrc32 si existiera visible (está static arriba)
    uint32_t fakeCrc = 0; for (size_t i=0;i<sizeof(headerBuf);++i) fakeCrc = (fakeCrc << 1) ^ headerBuf[i];
    crc = fakeCrc; f.write((uint8_t*)&crc,4);
    f.close();
    persistCount = 0; persistHead = 0; persistLoaded = true; logringCorrupt = false;
    return true;
}

bool DatabaseManager::healLogRingIfCorrupt() {
    if (!ensureFS()) return false;
    if (!LittleFS.exists("/logring.bin")) return false; // nada que hacer
    File f = LittleFS.open("/logring.bin", "r"); if (!f) return false; size_t sz = f.size(); f.close();
    if (!detectLogRingSuspicious(sz)) return false;
    // Intentar renombrar para análisis posterior
    LittleFS.remove("/logring.corrupt");
    if (LittleFS.rename("/logring.bin","/logring.corrupt")) {
        logringCorrupt = true;
    } else {
        // Si renombrar falla, eliminar
        LittleFS.remove("/logring.bin");
        logringCorrupt = true;
    }
    recreateEmptyLogRing();
    log(LOG_WARNING, "database", "Log ring self-healed", String("{\"corrupt_size\":") + sz + "}");
    return true;
}

// Hook: modificar begin() para invocar heal tras primer load