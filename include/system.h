#ifndef SYSTEM_H
#define SYSTEM_H

#include "config.h"
// Eliminado WiFiManager: conexión directa con credenciales fijas.
class NTPClient;
class WiFiUDP;

// NOTA: Eliminados includes pesados (WiFi.h, ArduinoOTA.h, etc.) del header.
// Si algún consumidor necesita tipos concretos, incluirlos en su propio .cpp.

class SystemManager {
private:
    // WiFiManager eliminado: ya no se necesita puntero
    NTPClient* ntpClient;
    WiFiUDP* ntpUDP;
    
    SystemState currentState;
    unsigned long bootTime;
    unsigned long lastHeartbeat; // Puede eliminarse si no se expone externamente (retenido por ahora)
    // Boot epoch (seconds since epoch) saved when NTP sync occurs or restored
    unsigned long bootEpoch;    // 0 if unknown
    unsigned long bootMillis;   // millis() at boot recorded for relative time calculations
    
    // Hardware adicional
    bool pauseButtonPressed;
    bool lastPauseButtonState;
    unsigned long pauseButtonDebounceTime;
    
    
    unsigned long statusLedLastBlink;
    bool statusLedState;
    bool statusLedEnabled;
    // Métricas de loop (EMA y última duración)
    unsigned long lastLoopMicros = 0;
    float loopEmaUs = 0.0f;
    unsigned long loopMeasureInterval = 0;
    unsigned long lastLoopDeltaUs = 0; // duración del último loop

    // Máquina de estados de reconexión WiFi (no bloqueante)
    enum WifiReconnectState { WIFI_IDLE, WIFI_LOST, WIFI_RETRY_WAIT, WIFI_RECONNECTING };
    WifiReconnectState wifiReconnectState = WIFI_IDLE;
    unsigned long wifiRetryStart = 0;
    uint8_t wifiRetryCount = 0;
    const uint8_t wifiMaxRetries = 3; // después de esto ampliar el intervalo
    unsigned long wifiBackoffMs = 5000; // se incrementa exponencialmente hasta un límite
    const unsigned long wifiBackoffMax = 30000; // 30s max entre intentos (antes 60s)
    unsigned long wifiLostTimestamp = 0; // timestamp cuando se perdió WiFi
    bool autonomousMode = false; // modo autónomo sin WiFi

    // Métricas de conectividad / NTP
    unsigned long wifiReconnectAttempts = 0;
    unsigned long wifiReconnectSuccesses = 0;
    unsigned long ntpFailureCount = 0;
    // WiFi disconnect reason tracking
    static const uint8_t WIFI_REASON_TRACKED_MIN = 1; // starting code
    static const uint8_t WIFI_REASON_TRACKED_MAX = 204; // track up to last defined we care
    static const size_t WIFI_REASON_BUCKETS = 205; // 0..204
    unsigned long wifiReasonCounters[WIFI_REASON_BUCKETS] = {0};
    uint8_t lastWifiDisconnectReason = 0;
    uint32_t minFreeHeap = UINT32_MAX; // seguimiento mínimo de heap
    uint32_t largestFreeBlock = 0; // métrica fragmentación (último valor)
    uint32_t minLargestFreeBlock = UINT32_MAX; // mínimo observado
    float fragmentationRatio = 0.0f; // largest_free_block / free_heap (último)
    float minFragmentationRatio = 0.0f; // mínimo observado
    
public:
    SystemManager();
    ~SystemManager();
    
    bool begin();
    void update(); // Llamar en loop principal
    void updateWifiReconnect(); // Nueva función de reconexión no bloqueante
    
    // WiFi y conectividad
    bool initWiFi();
    bool isWiFiConnected();
    void resetWiFiConfig();
    
    // NTP y tiempo
    bool initNTP();
    unsigned long getCurrentTimestamp();
    String getCurrentTimeString();
    bool isTimeSync();
    
    // OTA Updates
    bool initOTA();
    void handleOTA();
    
    // Watchdog
    bool initWatchdog();
    void feedWatchdog();
    void disableWatchdog();
    
    // Estado del sistema
    void setState(SystemState state);
    SystemState getState();
    String getStateString();
    
    // Hardware físico
    void updatePauseButton();
    bool isPauseButtonPressed();
    
    void updateStatusLed();
    void setStatusLed(bool fast_blink = false);
    void disableStatusLed();
    void enableStatusLed();
    
    // Detección de corte de luz
    void handlePowerRestoration();
    bool wasPowerLost();
    
    // Sistema de heartbeat
    void sendHeartbeat();
    unsigned long getUptime();
    float getLoopAvgMicros();
    unsigned long getLastLoopMicros();
    uint32_t getMinFreeHeap() const { return (minFreeHeap == UINT32_MAX) ? ESP.getFreeHeap() : minFreeHeap; }
    uint32_t getLargestFreeBlock() const { return largestFreeBlock; }
    uint32_t getMinLargestFreeBlock() const { return (minLargestFreeBlock == UINT32_MAX) ? largestFreeBlock : minLargestFreeBlock; }
    float getFragmentationRatio() const { return fragmentationRatio; }
    float getMinFragmentationRatio() const { return minFragmentationRatio; }

    // Métricas de conectividad / NTP
    unsigned long getWifiReconnectAttempts() const { return wifiReconnectAttempts; }
    unsigned long getWifiReconnectSuccesses() const { return wifiReconnectSuccesses; }
    unsigned long getNtpFailureCount() const { return ntpFailureCount; }
    uint8_t getLastWifiDisconnectReason() const { return lastWifiDisconnectReason; }
    unsigned long getWifiReasonCount(uint8_t reason) const { return (reason < WIFI_REASON_BUCKETS) ? wifiReasonCounters[reason] : 0; }
    void incrementWifiReason(uint8_t reason) { if (reason < WIFI_REASON_BUCKETS) { wifiReasonCounters[reason]++; lastWifiDisconnectReason = reason; } }
    
    // Información del sistema
    String getSystemInfo();
    
    // Modo autónomo
    bool isAutonomousMode() const { return autonomousMode; }
    void checkHeapHealth();
    void checkScheduledRestart();
    
private:
    void setupPins();
    void loadSystemState();
    void saveSystemState();
    
    // WiFiManager callbacks eliminados
    
    String lastError;
};

extern SystemManager system_manager;

#endif // SYSTEM_H