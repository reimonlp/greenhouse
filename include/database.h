#ifndef DATABASE_H
#define DATABASE_H

#include "config.h"
#include "sensors.h" // Necesitamos SensorData y SystemStats
#include "relays.h"   // Necesitamos RelayMode
#include <ArduinoJson.h> // Tipos JSON usados en la interfaz pública
// WiFi / HTTP / LittleFS movidos a database.cpp para reducir acoplamiento

struct LogEntry {
    unsigned long timestamp;
    LogLevel level;
    char source[16];    // Identificador corto del módulo
    char message[64];   // Mensaje principal truncado si excede
    char data[128];     // Payload JSON u otra info resumida
};

class DatabaseManager {
private:
    String mongoDbUri;
    String databaseName;
    String collectionSensors;
    String collectionLogs;
    String collectionStats;
    
    // Buffer local para logs
    LogEntry logBuffer[LOG_BUFFER_SIZE];
    int bufferIndex;
    bool bufferFull;
    unsigned long lastSendTime;
    
    // Backup local
    bool localBackupEnabled;
    String backupFilename;
    
    
public:
    DatabaseManager();
    ~DatabaseManager();
    
    bool begin();
    bool isConnected();
    
    // Configuración de conexión
    void setConnectionString(const String& uri, const String& database);
    void enableLocalBackup(bool enable);
    
    // Logging
    void log(LogLevel level, const String& source, const String& message, const String& data = "");
    void logSensorData(const SensorData& data);
    void logRelayAction(int relayIndex, bool state, RelayMode mode, const String& reason);
    void logSystemEvent(const String& event, const String& details = "");
    void logError(const String& source, const String& error);
    void logPowerEvent(bool powerLost, unsigned long timestamp);
    
    // Envío de datos
    bool sendLogBuffer();
    bool sendSensorData(const SensorData& data);
    bool sendSystemStats(const SystemStats& stats);
    void periodic(); // Lógica periódica (flush con jitter y batch)
    
    // Consultas
    String getRecentLogs(int count = 50);
    String getSensorHistory(unsigned long fromTime, unsigned long toTime);
    String getSystemStatistics(unsigned long fromTime, unsigned long toTime);
    
    // Backup local
    bool saveToLocalBackup();
    bool loadFromLocalBackup();
    String getLocalLogs(int count = 100);
    bool clearLocalLogs();
    
    // Mantenimiento
    void clearBuffer();
    int getBufferUsage();
    String getLastError();
    
    // Control runtime del nivel de log (silenciamiento dinámico)
    void setRuntimeLogLevel(LogLevel level);
    LogLevel getRuntimeLogLevel() const;
    
    // Buffer crítico (ERROR/CRITICAL) reciente
    String getCriticalLogs(int count = 25);
    
private:
    // Conexión MongoDB
    bool sendToMongoDB(const String& collection, const String& jsonData);
    String queryFromMongoDB(const String& collection, const String& query);
    
    // Backup local en LittleFS
    bool writeToLocalFile(const String& filename, const String& data, bool append = true);
    String readFromLocalFile(const String& filename);
    
    // Utilities
    String createLogJSON(const LogEntry& entry);
    String createSensorJSON(const SensorData& data);
    String createStatsJSON(const SystemStats& stats);
    unsigned long getCurrentTimestamp();
    
    // HTTP helpers
    bool makeHTTPRequest(const String& url, const String& payload, String& response);
    String buildMongoDBURL(const String& collection, const String& operation = "");
    
    String lastError;
    bool connected;
    LogLevel runtimeLogLevel = (LogLevel)MIN_LOG_LEVEL; // Nivel dinámico (por defecto igual al compilado)
    static const uint8_t CRIT_BUFFER_SIZE = 25;
    LogEntry criticalBuffer[CRIT_BUFFER_SIZE];
    uint8_t criticalIndex = 0;
    bool criticalFull = false;
    unsigned long nextPlannedFlush = 0; // tiempo absoluto (millis) para siguiente flush planificado

    // ==== Persisted Log Ring (ERROR/CRITICAL across resets) ====
    static const uint16_t PERSIST_LOG_RING_CAPACITY = 64; // tunable (each entry ~66 bytes)
    struct PersistRingEntry {
        uint32_t ts;
        uint8_t lvl;
        char src[12];
        char msg[48];
        uint8_t pad; // alignment / future
    };
    PersistRingEntry persistRing[PERSIST_LOG_RING_CAPACITY];
    uint16_t persistCount = 0; // number of valid entries
    uint16_t persistHead = 0;  // next write position
    bool persistLoaded = false;
    void loadPersistedLogRing();
    void persistLogRingEntry(const LogEntry& entry);
    void seedCriticalBufferFromRing();
    // --- Self-heal helpers ---
    bool detectLogRingSuspicious(size_t fileSize) const; // heurística para decidir si un archivo con tamaño >0 pero sin entradas es sospechoso
    bool healLogRingIfCorrupt(); // intenta renombrar y recrear logring si está corrupto
    bool recreateEmptyLogRing();
    bool logringCorrupt = false; // flag para saber si se detectó corrupción y se sanó
};

extern DatabaseManager database;

#endif // DATABASE_H